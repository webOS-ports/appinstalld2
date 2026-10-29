// Copyright (c) 2026 Herman van Hazendonk <github.com@herrie.org>
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

#ifndef PERMISSIONFILTER_H
#define PERMISSIONFILTER_H

#include <set>
#include <string>
#include <vector>

#include <pbnjson.hpp>

/**
 * Decides which LS2 access control groups an application installed by
 * appinstalld may hold.
 *
 * An application asks for groups in "requiredPermissions" in its own
 * appinfo.json, which the application writes itself. appinstalld verifies no
 * signature, so nothing it installs is trusted: it keeps only the groups listed
 * in the third-party allowlist (third-party-grantable.json), and never a group
 * that isAlwaysDenied() matches, whatever the allowlist says.
 */
namespace PermissionFilter {

//! True for groups no installed application may hold: "all", "private",
//! "*.management", "*.devmode", "*.devutility", "*.debug", "*.internal",
//! "*.profiling", "auth.*", "devmode*", "applicationinstall.*", "arescli.*".
bool isAlwaysDenied(const std::string &group);

//! Read the allowlist ({"grantable": ["group", ...]}) from path into out.
//! Returns false, with out empty, when the file is missing or malformed; the
//! caller then grants nothing from requiredPermissions (fail closed).
bool loadAllowlist(const std::string &path, std::set<std::string> &out);

//! The groups of requested that are in allowlist and not isAlwaysDenied(), in
//! the order requested, each once. The others are appended to dropped when it
//! is not null.
std::vector<std::string> filter(const std::vector<std::string> &requested,
                                const std::set<std::string> &allowlist,
                                std::vector<std::string> *dropped = nullptr);

//! The strings of an appinfo.json "requiredPermissions" value. Returns false,
//! with out empty, unless it is an array holding only strings.
bool requestedGroups(const pbnjson::JValue &requiredPermissions, std::vector<std::string> &out);

//! filter(), but never empty: an application left with nothing gets "public",
//! as one without requiredPermissions does.
std::vector<std::string> grantedGroups(const std::vector<std::string> &requested,
                                       const std::set<std::string> &allowlist,
                                       std::vector<std::string> *dropped = nullptr);

} // namespace PermissionFilter

#endif // PERMISSIONFILTER_H
