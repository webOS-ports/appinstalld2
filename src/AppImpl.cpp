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

#include "AppImpl.h"

#include <glib-unix.h>

#include "client/SessionManager.h"
#include "service/AppInstallService.h"
#include "settings/Settings.h"
#include "util/Logger.h"

using namespace std::placeholders;

gboolean AppImpl::term_handler(gpointer user_data)
{
    // dispatched from the main loop by g_unix_signal_add, so calling into
    // glib/luna-service2 here is safe (unlike a raw signal handler)
    MainApp::instance().quit();
    return G_SOURCE_REMOVE;
}

AppImpl::AppImpl()
{
    setClassName("AppImpl");
}

bool AppImpl::onCreate()
{
    g_unix_signal_add(SIGTERM, AppImpl::term_handler, NULL);

    AppInstallService::getInstance().attach(mainLoop());
    SessionManager::getInstance().initialize();

    return true;
}

bool AppImpl::onDestroy()
{
    SessionManager::getInstance().finalize();
    AppInstallService::getInstance().detach();

    return true;
}
