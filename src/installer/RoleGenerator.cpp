// Copyright (c) 2013-2025 LG Electronics, Inc.
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

#include "installer/RoleGenerator.h"

#include <fstream>

#include "base/Logging.h"
#include "installer/PermissionFilter.h"

namespace {

//! Replace every occurrence of tag, resuming the search after each inserted
//! value so a value containing the tag itself cannot loop forever.
void replaceTag(std::string &line, const std::string &tag, const std::string &value)
{
    size_t n = 0;
    while ((n = line.find(tag, n)) != std::string::npos) {
        line.replace(n, tag.length(), value);
        n += value.length();
    }
}

} // namespace

namespace RoleGenerator {

bool generate(const std::string &templatePath,
              const std::string &destinationPath,
              const std::string &id,
              const std::string &executablePath)
{
    // Everything appinstalld installs is untrusted: it verifies no package
    // signature, and system applications come with the image, not from here.
    // So the role is "dev" however the package was installed, never "oem".
    const std::string trustLevel = PermissionFilter::kInstalledAppTrustLevel;
    static const std::string ID_TAG = "XXXIDXXX";
    static const std::string PATH_TAG = "XXXEXEPATHXXX";
    static const std::string trustLevel_TAG = "XXXPERMLEVELXXX";

    std::ifstream myfile(templatePath.c_str());
    if (!myfile.is_open()) {
        LOG_WARNING(MSGID_WRONG_SERVICEID, 1,
                    PMLOGKS("TEMPLATE", templatePath.c_str()),
                    "Cannot open role template");
        return false;
    }

    std::ofstream newfile(destinationPath.c_str());
    if (!newfile.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(myfile, line)) {
        replaceTag(line, ID_TAG, id);
        replaceTag(line, PATH_TAG, executablePath);
        replaceTag(line, trustLevel_TAG, trustLevel);
        newfile << line << std::endl;
    }
    newfile.close();
    myfile.close();
    return true;
}

} // namespace RoleGenerator
