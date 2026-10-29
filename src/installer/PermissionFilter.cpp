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

#include "installer/PermissionFilter.h"

namespace PermissionFilter {

namespace {

bool startsWith(const std::string &s, const char *prefix)
{
    return s.compare(0, std::char_traits<char>::length(prefix), prefix) == 0;
}

bool endsWith(const std::string &s, const char *suffix)
{
    const size_t n = std::char_traits<char>::length(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

} // namespace

bool isAlwaysDenied(const std::string &group)
{
    static const char * const exact[] = { "all", "private" };
    static const char * const prefixes[] = {
        "auth.", "devmode", "applicationinstall.", "arescli.",
    };
    static const char * const suffixes[] = {
        ".management", ".devmode", ".devutility", ".debug", ".internal", ".profiling",
    };

    for (const char *e : exact)
        if (group == e)
            return true;
    for (const char *p : prefixes)
        if (startsWith(group, p))
            return true;
    for (const char *s : suffixes)
        if (endsWith(group, s))
            return true;
    return false;
}

bool loadAllowlist(const std::string &path, std::set<std::string> &out)
{
    out.clear();

    pbnjson::JValue root = pbnjson::JDomParser::fromFile(path.c_str());
    if (!root.isObject())
        return false;

    pbnjson::JValue grantable = root["grantable"];
    if (!grantable.isArray())
        return false;

    std::set<std::string> groups;
    for (pbnjson::JValue item : grantable.items()) {
        if (!item.isString())
            return false;
        groups.insert(item.asString());
    }

    out.swap(groups);
    return true;
}

std::vector<std::string> filter(const std::vector<std::string> &requested,
                                const std::set<std::string> &allowlist,
                                std::vector<std::string> *dropped)
{
    std::vector<std::string> kept;
    std::set<std::string> seen;

    for (const auto &group : requested) {
        if (!seen.insert(group).second)
            continue;
        if (!isAlwaysDenied(group) && allowlist.count(group))
            kept.push_back(group);
        else if (dropped)
            dropped->push_back(group);
    }
    return kept;
}

bool requestedGroups(const pbnjson::JValue &requiredPermissions, std::vector<std::string> &out)
{
    out.clear();
    if (!requiredPermissions.isArray())
        return false;

    std::vector<std::string> groups;
    for (pbnjson::JValue item : requiredPermissions.items()) {
        if (!item.isString())
            return false;
        groups.push_back(item.asString());
    }
    out.swap(groups);
    return true;
}

std::vector<std::string> grantedGroups(const std::vector<std::string> &requested,
                                       const std::set<std::string> &allowlist,
                                       std::vector<std::string> *dropped)
{
    std::vector<std::string> kept = filter(requested, allowlist, dropped);
    if (kept.empty())
        kept.push_back("public");
    return kept;
}

} // namespace PermissionFilter
