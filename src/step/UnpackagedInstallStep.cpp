// Copyright (c) 2017-2023 LG Electronics, Inc.
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

#include "UnpackagedInstallStep.h"
#include <functional>
#include <glob.h>
#include <string.h>
#include <unistd.h>
#include "installer/AppInfo.h"
#include "installer/Task.h"
#include "settings/Settings.h"

UnpackagedInstallStep::UnpackagedInstallStep()
{
}

UnpackagedInstallStep::~UnpackagedInstallStep()
{
    LOG_DEBUG("UnpackagedInstallStep::~UnpackagedInstallStep called\n");
}

bool UnpackagedInstallStep::proceed(Task *task)
{

    static std::string app_path = Settings::instance().getInstallApplicationPath(true) + "/";
    static std::string pack_path = Settings::instance().getInstallPackagePath(true) + "/";
    enum
    {
        MKDIR_APP = 1,
        COPY_ICON,
        COPY_APPINFO
    };

    LOG_DEBUG("UnpackagedInstallStep::%s() called\n", __FUNCTION__);

    m_parentTask = task;
    pbnjson::JValue param = m_parentTask->getParam();
    std::string unpackaged_data = Utils::getPWAPath(param["ipkurl"].asString());
    m_appId = m_parentTask->getAppId();
    m_parentTask->setUnpackFilesize(Utils::dir_size(unpackaged_data));

    bool isUpdate = (-1 != Utils::file_size(app_path + m_appId));

    if (!checkStorageSize())
    {
        task->setError(ErrorInstall, APP_INSTALL_ERR_DISKFULL, "There is no available space to install App");
        task->proceed();
        return false;
    }

    AppInfo newAppInfo{unpackaged_data};
    if (!newAppInfo.isLoaded())
    {
        m_parentTask->setError(ErrorInstall, APP_INSTALL_ERR_INSTALL, "Failed to parse appinfo.json");
        m_parentTask->proceed();
        return false;
    }

    if (m_appId != newAppInfo.getId())
    {
        m_parentTask->setError(ErrorInstall, APP_INSTALL_ERR_INSTALL, "appId is wrong");
        m_parentTask->proceed();
        return false;
    }

    if (isUpdate)
    {
        AppInfo installedAppInfo{app_path + m_appId};
        if (installedAppInfo.isLoaded())
        {
            if (newAppInfo.getVersion() == installedAppInfo.getVersion())
                m_parentTask->setAllowReInstall(true);
        }
        Utils::remove_dir(app_path + m_appId);
    }

    int step = MKDIR_APP;
    if (Utils::make_dir(app_path + m_appId + "/", true))
    {
        // Copy the icons and appinfo.json ourselves; the source directory
        // comes straight from the caller's ipkUrl, so no shell may ever see it
        std::string destDir = app_path + m_appId + "/";

        step = COPY_ICON;
        bool copiedIcons = true;
        glob_t iconGlob;
        memset(&iconGlob, 0, sizeof(iconGlob));
        int globResult = glob((unpackaged_data + "/icon*").c_str(), 0, nullptr, &iconGlob);
        if (globResult == 0) {
            for (size_t i = 0; i < iconGlob.gl_pathc; ++i) {
                std::string srcFile = iconGlob.gl_pathv[i];
                std::string baseName = srcFile.substr(srcFile.find_last_of('/') + 1);
                if (!Utils::copy_file(srcFile, destDir + baseName)) {
                    copiedIcons = false;
                    break;
                }
            }
        } else if (globResult != GLOB_NOMATCH) {
            copiedIcons = false;
        }
        globfree(&iconGlob);

        if (copiedIcons)
        {
            step = COPY_APPINFO;
            if (Utils::copy_file(unpackaged_data + "/appinfo.json", destDir + "appinfo.json"))
            {
                LOG_DEBUG("UnpackagedInstallStep::%s() succses\n", __FUNCTION__);
                sync();
                task->setUnpacked(true);
                task->setStep(IpkInstallComplete);
                if (isUpdate)
                    m_parentTask->setOriginAppInfo("{}");
                m_parentTask->proceed();

                return true;
            }
        }
    }
    std::string error_message;
    switch (step)
    {
    case MKDIR_APP:
        error_message = "mkdir " + app_path + m_appId + " error";
        break;
    case COPY_ICON:
        error_message = "copy icons error";
        break;
    case COPY_APPINFO:
        error_message = "copy appinfo.json error";
        break;
    }

    task->setError(ErrorInstall, APP_INSTALL_ERR_GENERAL, std::move(error_message));
    return false;
}
