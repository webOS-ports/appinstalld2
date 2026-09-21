// Copyright (c) 2013-2019 LG Electronics, Inc.
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

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <glib.h>
#include <memory.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Logging.h"
#include "Utils.h"

std::string Utils::read_file(const std::string &path)
{
    std::ifstream file(path.c_str());
    if (file.good()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        return buffer.str();
    }

    return "";
}

bool Utils::make_dir(const std::string &path, bool withParent)
{
    if (!withParent) {
        int result = mkdir(path.c_str(), 0755);
        if (result == 0)
            return true;
        if (errno == EEXIST) {
            // Only accept a pre-existing plain directory; a file or symlink
            // sitting at the path must not count as success.
            struct stat st;
            return lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        }
    } else {
        int result = g_mkdir_with_parents(path.c_str(), 0755);
        if (result == 0)
            return true;
    }

    return false;
}

static int rmdir_helper(const char *path, const struct stat *pStat, int flag, struct FTW *ftw)
{
    switch(flag)
    {
        case FTW_D:
        case FTW_DP:
            if (::rmdir(path) == -1)
                return -1;
            break;

        case FTW_F:
        case FTW_SL:
            if (::unlink(path) == -1)
                return -1;
            break;
    }

    return 0;
}

bool Utils::remove_dir(const std::string &path)
{
    struct stat oStat;
    memset(&oStat, 0, sizeof(oStat));
    if (lstat(path.c_str(), &oStat) == -1)
        return false;

    if (S_ISDIR(oStat.st_mode)) {
        /* FTW_PHYS: For signage, download path are linked to the directory */
        int flags = FTW_DEPTH | FTW_PHYS;
        if (::nftw(path.c_str(), rmdir_helper, 10, flags) == -1) {
            return false;
        }
    } else {
        return false;
    }

    return true;
}

bool Utils::remove_file(const std::string &path)
{
    if (::unlink(path.c_str()) == -1)
        return false;
    return true;
}

bool Utils::copy_file(const std::string &srcPath, const std::string &destPath)
{
    std::ifstream src(srcPath.c_str(), std::ios::binary);
    if (!src.good())
        return false;

    std::ofstream dest(destPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!dest.good())
        return false;

    dest << src.rdbuf();
    return dest.good();
}

long long Utils::file_size(const std::string &path)
{
    struct stat buf;
    if (-1 == stat(path.c_str(), &buf))
        return -1;
    return (long long)buf.st_size;
}

bool Utils::isPWA(const std::string &path)
{
    return (0 == path.find("pwa://"));
}

std::string Utils::getPWAPath(const std::string &path)
{
    return path.substr(6); //returning after PWA:://
}

long long Utils::dir_size(const std::string &path)
{
    DIR *d = opendir(path.c_str());
    if (d == NULL)
        return 0;

    struct dirent *de;
    struct stat buf;
    long long total_size = 0;
    for (de = readdir(d); de != NULL; de = readdir(d)) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        std::string entryPath = path + "/" + de->d_name;
        if (-1 == lstat(entryPath.c_str(), &buf))
            continue;

        if (S_ISDIR(buf.st_mode))
            total_size += dir_size(entryPath);
        else
            total_size += buf.st_size;
    }
    closedir(d);
    return total_size;
}
bool Utils::is_File_exist(const std::string &path)
{
    struct stat buf;
    if (0 != stat(path.c_str(), &buf))
        return false;
    return true;
}

bool Utils::isValidAppId(const std::string &appId)
{
    // webOS application ids are reverse-dns style: lowercase letters, digits,
    // '.', '-', '+' and '_', never starting with a separator and never
    // containing '/' or ".." (they end up in filesystem paths).
    if (appId.empty() || appId.size() > 256)
        return false;

    char first = appId.front();
    if (!g_ascii_isalnum(first))
        return false;

    for (char c : appId) {
        if (!(g_ascii_isalnum(c) || c == '.' || c == '-' || c == '_' || c == '+'))
            return false;
    }

    if (appId.find("..") != std::string::npos)
        return false;

    return true;
}
gboolean Utils::cbAsync(gpointer data)
{
    IAsyncCall *p = reinterpret_cast<IAsyncCall*>(data);
    if (!p) return false;

    // an exception must not unwind through the glib C dispatch code,
    // and p must be freed either way
    try {
        p->Call();
    } catch (const std::exception &e) {
        LOG_WARNING(MSGID_LSCALL_ERR, 1,
                    PMLOGKS(LOGKEY_ERRTEXT, e.what()),
                    "Unhandled exception in deferred call");
    } catch (...) {
        LOG_WARNING(MSGID_LSCALL_ERR, 0, "Unhandled exception in deferred call");
    }

    delete p;

    return false;
}
