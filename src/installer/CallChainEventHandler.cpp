// Copyright (c) 2013-2020 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include <boost/algorithm/string/replace.hpp>

#include "AppInstallerUtilityErrors.h"
#include "base/JUtil.h"
#include "base/Utils.h"
#include "base/LSUtils.h"
#include "base/Logging.h"
#include "CallChainEventHandler.h"
#include "settings/Settings.h"
#include "PackageInfo.h"
#include "ServiceInfo.h"

using namespace std::placeholders;

namespace CallChainEventHandler
{
    const int SETTINGSERVICE_GET_VALUE_NUM = 3;

    AppRunning::AppRunning(const char *serviceName, const char *sessionId, std::string id)
        : LSCallItem(serviceName, "luna://com.webos.applicationManager/running", "{}", sessionId),
          m_id(std::move(id))
    {
    }

    bool AppRunning::onReceiveCall(pbnjson::JValue message)
    {
        bool returnValue = message["returnValue"].asBool();
        if (!returnValue) {
            setError("Get running list failed");
            return false;
        }

        pbnjson::JValue runningApps = message["running"];
        if (!runningApps.isArray()) {
            setError("Invalid running list format");
            return false;
        }

        pbnjson::JValue app;
        int arraySize = runningApps.arraySize();
        for(int i = 0 ; i < arraySize ; ++i) {
            app = runningApps[i];
            if (app["id"].asString() == m_id)
                return true;
        }

        return false;
    }

    AppClose::AppClose(const char *serviceName, const char *sessionId, std::string id)
        : LSCallItem(serviceName, "luna://com.webos.applicationManager/closeByAppId", "", sessionId)
    {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("id", id);
        setPayload(JUtil::toSimpleString(std::move(payload)).c_str());
    }

    bool AppClose::onReceiveCall(pbnjson::JValue message)
    {
        bool returnValue = message["returnValue"].asBool();
        if (!returnValue) {
            setError("Close app failed");
            return false;
        }

        return true;
    }

    AppInfo::AppInfo(const char *serviceName, const char* sessionId, std::string id)
        : LSCallItem(serviceName, "luna://com.webos.applicationManager/getAppInfo", "", sessionId)
    {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("id", id);
        setPayload(JUtil::toSimpleString(std::move(payload)).c_str());
    }

    bool AppInfo::onReceiveCall(pbnjson::JValue message)
    {
        bool returnValue = message["returnValue"].asBool();
        if (returnValue) {
            pbnjson::JValue chainData = getChainData();
            chainData.put("appInfo", message["appInfo"]);
            setChainData(chainData);
            return true;
        }

        return false;
    }

    AppRemovable::AppRemovable()
    {
    }

    bool AppRemovable::Call()
    {
        pbnjson::JValue chainData = getChainData();
        pbnjson::JValue appInfo = chainData["appInfo"];
        if (appInfo.isNull() || !appInfo["removable"].asBool()) {
            onError("The app is not removable");
            return false;
        }

        onFinished(true, "");
        return true;
    }

    AppLock::AppLock(const char *serviceName, const char *sessionId, std::string id)
        : LSCallItem(serviceName, "luna://com.webos.applicationManager/lockApp", "", sessionId)
    {
        pbnjson::JValue payload = pbnjson::Object();
        payload.put("id", id);
        payload.put("lock", true);
        setPayload(JUtil::toSimpleString(std::move(payload)).c_str());
    }

    bool AppLock::onReceiveCall(pbnjson::JValue message)
    {
        bool returnValue = message["returnValue"].asBool();
        if (!returnValue) {
            setError("Lock app failed");
            return false;
        }

        pbnjson::JValue chainData = getChainData();
        chainData.put("id", message["id"].asString());
        setChainData(std::move(chainData));

        return true;
    }

    SvcClose::SvcClose(const char *sessionId)
        : m_numResponse(0),
          m_numServices(0),
          m_signaled(false),
          m_hasSessionId(sessionId != nullptr),
          m_sessionId(sessionId != nullptr ? sessionId : "")
    {
    }

    void SvcClose::signalFinished(bool result, std::string errorText)
    {
        // several /quit replies may race to complete this item;
        // the chain must only ever be finished once
        if (m_signaled)
            return;
        m_signaled = true;

        auto self = std::static_pointer_cast<SvcClose>(shared_from_this());
        Utils::async([self, result, errorText = std::move(errorText)] {
            self->onFinished(result, errorText);
        });
    }

    bool SvcClose::Call()
    {
        pbnjson::JValue chainData = getChainData();
        if (chainData.isNull()) {
            signalFinished(true, "");
            return true;
        }

        pbnjson::JValue appInfo = chainData["appInfo"];
        if (appInfo.isNull()) {
            signalFinished(true, "");
            return true;
        }

        std::string appPath = appInfo["folderPath"].asString();
        std::string appId = appInfo["id"].asString();
        std::string appDir = Settings::instance().getApplicationPath();

        std::string packagePath = boost::replace_all_copy(appPath, appDir, Settings::instance().getPackagePath());
        LOG_DEBUG("[SVC_CLOSE] package path : %s", packagePath.c_str());

        PackageInfo packageInfo(std::move(packagePath));

        std::vector<std::string> serviceLists;
        packageInfo.getServices(serviceLists);

        m_numServices = static_cast<int>(serviceLists.size());
        for(auto iter = serviceLists.begin(); iter != serviceLists.end(); ++iter) {
            std::string servicePath = boost::replace_all_copy(appPath, appDir + "/" + appId, std::string("/usr/palm/services/") + (*iter));
            LOG_DEBUG("[SVC_CLOSE] service path : %s", servicePath.c_str());

            ServiceInfo serviceInfo(std::move(servicePath));
            if (serviceInfo.getType() != "native") {
                std::string errorText;
                std::string uri = "luna://" + serviceInfo.getId() + "/quit";
                LSCaller caller = LSUtils::acquireCaller("com.webos.appInstallService");
                LOG_DEBUG("[NODEJS_SVC_CLOSE] uri : %s, session : %s", uri.c_str(), m_hasSessionId ? m_sessionId.c_str() : "(nullptr)");
                // each in-flight callback keeps this item alive until it runs
                auto *holder = new std::shared_ptr<SvcClose>(
                    std::static_pointer_cast<SvcClose>(shared_from_this()));
                if (!caller.CallOneReply(uri.c_str(), "{}",
                                         m_hasSessionId ? m_sessionId.c_str() : nullptr,
                                         cbQuit, holder, NULL, errorText)) {
                    delete holder;
                    signalFinished(false, std::move(errorText));
                    break;
                }
            } else {
                // serviceExec comes from the package's own services.json:
                // never pass it through a shell
                std::string serviceExec = serviceInfo.getExec(true);
                LOG_DEBUG("[NATIVE_SVC_CLOSE] pkill -f %s", serviceExec.c_str());

                gchar *argv[] = {
                    const_cast<gchar *>("pkill"),
                    const_cast<gchar *>("-f"),
                    const_cast<gchar *>(serviceExec.c_str()),
                    nullptr
                };
                GError *spawnError = nullptr;
                if (!g_spawn_sync(nullptr, argv, nullptr,
                                  (GSpawnFlags)(G_SPAWN_SEARCH_PATH |
                                                G_SPAWN_STDOUT_TO_DEV_NULL |
                                                G_SPAWN_STDERR_TO_DEV_NULL),
                                  nullptr, nullptr, nullptr, nullptr,
                                  nullptr, &spawnError)) {
                    LOG_WARNING(MSGID_APP_INSTALL_ERR, 1,
                                PMLOGKS(LOGKEY_ERRTEXT,
                                        spawnError ? spawnError->message : "unknown"),
                                "Failed to close native service");
                    g_clear_error(&spawnError);
                }

                ++m_numResponse;
            }
        }

        if (m_numServices == m_numResponse) {
            signalFinished(true, "");
        }

        return true;
    }

    bool SvcClose::cbQuit(LSHandle *lshandle, LSMessage *msg, void *user_data)
    {
        auto *holder = static_cast<std::shared_ptr<SvcClose>*>(user_data);
        if (!holder)
            return false;

        std::shared_ptr<SvcClose> item = *holder;
        delete holder;
        if (!item)
            return false;

        const char *payload = LSMessageGetPayload(msg);
        pbnjson::JValue json = JUtil::parse(payload ? payload : "", std::string(""));
        bool returnValue = json["returnValue"].asBool();

        if (!returnValue) {
            std::string errorText = json["errorText"].asString();
            if (errorText.empty())
                errorText = "Failed to quit nodejs service";

            item->signalFinished(false, std::move(errorText));

            return true;
        }

        ++(item->m_numResponse);
        if (item->m_numResponse == item->m_numServices) {
            item->signalFinished(true, "");
        }

        return true;
    }

    RemoveDb::RemoveDb(const char* serviceName, const char* sessionId, pbnjson::JValue owners)
        : LSCallItem(serviceName, "luna://com.webos.service.db/removeAppData", "", sessionId)
    {
        pbnjson::JValue json = pbnjson::Object();
        json.put("owners", owners);
        setPayload(JUtil::toSimpleString(std::move(json)).c_str());
    }

    bool RemoveDb::onReceiveCall(pbnjson::JValue message)
    {
        return true;
    }

    UpdateManifest::UpdateManifest(const char *serviceName, bool isAdd, std::string path, std::string prefix, std::string id)
        : LSCallItem(serviceName, "", "{}")
    {
        if (isAdd)
            setUri("luna://com.webos.service.bus/addOneManifest");
        else
            setUri("luna://com.webos.service.bus/removeOneManifest");

        pbnjson::JValue payload = pbnjson::Object();
        payload.put("path", path + "/" + id + ".json" );
        if (prefix.empty())
            payload.put("prefix","/");
        else
            payload.put("prefix", prefix + "/");

        setPayload(JUtil::toSimpleString(std::move(payload)).c_str());
    }

    bool UpdateManifest::onReceiveCall(pbnjson::JValue message)
    {
        bool returnValue = message["returnValue"].asBool();
        if (!returnValue) {
            setError("update manifest file failed");
            return false;
        }
        return returnValue;
    }

    RemoveIpk::RemoveIpk(std::string id, bool verify, std::string externalPath)
        : m_id(id),
          m_verify(verify),
          m_externalPath(std::move(externalPath))
    {
    }

    bool RemoveIpk::Call()
    {
        AppInstallerUtility::Result result =
            m_installerUtility.remove(m_id,
                                      m_verify,
                                      m_externalPath,
                                      std::bind(&RemoveIpk::cbRemoveIpkProgress, this, _1),
                                      std::bind(&RemoveIpk::cbRemoveIpkComplete, this, _1));

        switch(result)
        {
            case AppInstallerUtility::FAIL:
                onError("unable to call ApplicationInstallerUtility");
                return false;
            case AppInstallerUtility::LOCKED: {
                pbnjson::JValue chainData = getChainData();
                chainData.put("locked", true);
                setChainData(chainData);
                onError("Opkg is locked");
                return false;
            }
            default:
                break;
        }

        return true;
    }

    void RemoveIpk::cbRemoveIpkProgress(const char *str)
    {
    }

    void RemoveIpk::cbRemoveIpkComplete(int status)
    {
        LOG_DEBUG("Remove Complete with %d", status);

        if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
            // WEXITSTATUS is only meaningful for a normal exit; a signal-killed
            // child must be treated as a failure, not decoded into error codes
            switch (WIFEXITED(status) ? WEXITSTATUS(status) : AI_ERR_REMOVE_FAILEDIPKGREMOVE)
            {
                case AI_ERR_INSTALL_TARGETNOTFOUND:
                    break;
                case AI_ERR_REMOVE_FAILEDIPKGREMOVE:
                default:
                    LOG_WARNING(MSGID_IPK_REMOVE_INFO, 2,
                                PMLOGKS(FUNCTION,__PRETTY_FUNCTION__),
                                PMLOGKFV(STATUS,"%d",status), "");

                    pbnjson::JValue chainData = getChainData();
                    int failed = 0;
                    if (chainData.hasKey("failed"))
                        failed = chainData["failed"].asNumber<int>();
                    ++failed;
                    chainData.put("failed", failed);
                    setChainData(chainData);
                    break;
            }
        } else {
            pbnjson::JValue chainData = getChainData();
            int removed = 0;
            if (chainData.hasKey("removed"))
                removed = chainData["removed"].asNumber<int>();
            ++removed;
            chainData.put("removed", removed);
            setChainData(chainData);
        }

        sync();
        onFinished(true, std::string(""));
    }
}
