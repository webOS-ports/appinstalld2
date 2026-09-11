// Copyright (c) 2017-2021  LG Electronics, Inc.
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

#include "ServiceUninstallStep.h"
#include "installer/Task.h"

using namespace std::placeholders;

ServiceUninstallStep::ServiceUninstallStep()
{
}

ServiceUninstallStep::~ServiceUninstallStep()
{
    LOG_DEBUG("ServiceUninstallStep::~ServiceUninstallStep() called\n");
}

bool ServiceUninstallStep::proceed(Task *task)
{
    LOG_DEBUG("ServiceUninstallStep::proceed() called\n");

    m_parentTask = task;

    pbnjson::JValue param = task->getParam();
    bool verify = param["verify"].asBool();

    ServiceInstallerUtility::PathInfos pathInfos;
    std::string lunaFilesPath;

    // the Settings getters return raw pointers that are NULL when the
    // corresponding directory is not configured; std::string(NULL) is UB
    auto makePathInfo = [](bool verified, const std::string &files,
                           ServiceInstallerUtility::PathInfo &out) -> bool {
        const char *roled = Settings::instance().getLunaUnifiedRolesDir(verified);
        const char *serviced = Settings::instance().getLunaUnifiedServicesDir(verified);
        const char *permissiond = Settings::instance().getLunaUnifiedPermissionsDir(verified, true);
        const char *apiPermissiond = Settings::instance().getLunaUnifiedAPIPermissionsDir(verified);
        const char *groupd = Settings::instance().getLunaUnifiedGroupsDir(verified);
        const char *manifestsd = Settings::instance().getLunaUnifiedManifestsDir(verified, true);
        if (!roled || !serviced || !permissiond || !apiPermissiond || !groupd || !manifestsd)
            return false;
        out = ServiceInstallerUtility::PathInfo {
            verified,
            "",
            files + std::string("/roles"),
            files + std::string("/services"),
            roled,
            serviced,
            permissiond,
            apiPermissiond,
            groupd,
            manifestsd
        };
        return true;
    };

    if (verify)
    {
        lunaFilesPath = Settings::instance().getLunaFilesPath(true);
        ServiceInstallerUtility::PathInfo info{};
        if (!makePathInfo(true, lunaFilesPath, info)) {
            LOG_DEBUG("[ServiceUninstallStep::proceed] luna dirs not configured");
            return false;
        }
        pathInfos.push_back(std::move(info));
    }

    // unverified internal
    lunaFilesPath = Settings::instance().getLunaFilesPath(false);
    {
        ServiceInstallerUtility::PathInfo info{};
        if (!makePathInfo(false, lunaFilesPath, info)) {
            LOG_DEBUG("[ServiceUninstallStep::proceed] luna dirs not configured");
            return false;
        }
        pathInfos.push_back(std::move(info));
    }

    for (unsigned i = 0; i < pathInfos.size(); i++) {
        ServiceInstallerUtility::PathInfo& pathInfo = pathInfos.at(i);
        LOG_DEBUG("[RemoveTask] install paths : pathInfo - root : %s, verified : %d, roles : %s, services : %s, roled : %s, serviced : %s, permissiond : %s, api_permissiond : %s, manifestsd : %s",
                  pathInfo.root.c_str(),
                  (int)pathInfo.verified,
                  pathInfo.legacyRoles.c_str(),
                  pathInfo.legacyServices.c_str(),
                  pathInfo.roled.c_str(),
                  pathInfo.serviced.c_str(),
                  pathInfo.permissiond.c_str(),
                  pathInfo.api_permissiond.c_str(),
                  pathInfo.manifestsd.c_str());
    }

    if (!m_svcinstallerUtility.remove(m_parentTask->getAppId(), pathInfos, std::bind(&ServiceUninstallStep::onUninstallServiceComplete, this, _1, _2)))
    {
        m_parentTask->setError(ErrorRemove, APP_REMOVE_ERR_GENERAL, "failed to remove service file");
        return false;
    }
    return true;
}

void ServiceUninstallStep::onUninstallServiceComplete(bool success, std::string errorText)
{
    LOG_DEBUG("ServiceUninstallStep::onUninstallServiceComplete() called\n");

    if(!success)
    {
        gchar* escaped_errtext = g_strescape(errorText.c_str(),NULL);
        LOG_WARNING(MSGID_UNINSTALL_SERVICE_FAIL,2,PMLOGKS(APP_ID, m_parentTask->getAppId().c_str()),
                                                    PMLOGKS(LOGKEY_ERRTEXT,escaped_errtext),"uninstall service failed");
        g_free(escaped_errtext);
    }

    m_parentTask->setStep(ServiceUninstallComplete);
    m_parentTask->proceed();
}
