// Copyright (c) 2015-2018 LG Electronics, Inc.
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "System.h"

//! Collect the direct children of pid by scanning /proc/<pid>/stat ppid fields
static void collectChildren(pid_t parent, std::vector<pid_t> &out)
{
    DIR *proc = opendir("/proc");
    if (proc == NULL)
        return;

    struct dirent *de;
    while ((de = readdir(proc)) != NULL) {
        char *end = NULL;
        long pid = strtol(de->d_name, &end, 10);
        if (end == de->d_name || *end != '\0' || pid <= 0)
            continue;

        char statPath[64];
        snprintf(statPath, sizeof(statPath), "/proc/%ld/stat", pid);
        FILE *fp = fopen(statPath, "r");
        if (fp == NULL)
            continue;

        // pid (comm) state ppid ... — comm may contain spaces/parens, so
        // parse from the last ')'
        char buf[512];
        pid_t ppid = -1;
        if (fgets(buf, sizeof(buf), fp) != NULL) {
            const char *p = strrchr(buf, ')');
            if (p != NULL)
                sscanf(p + 1, " %*c %d", &ppid);
        }
        fclose(fp);

        if (ppid == parent)
            out.push_back((pid_t)pid);
    }
    closedir(proc);
}

int System::kill(const unsigned int pid, bool recursive)
{
    if (pid == 0)
        return -1;

    if (recursive) {
        // breadth-first walk of the process tree rooted at pid
        std::vector<pid_t> tree;
        tree.push_back((pid_t)pid);
        for (size_t i = 0; i < tree.size() && tree.size() < 1024; ++i)
            collectChildren(tree[i], tree);

        // terminate leaves first so parents cannot respawn/reap them oddly
        int result = 0;
        for (auto it = tree.rbegin(); it != tree.rend(); ++it) {
            if (::kill(*it, SIGTERM) == -1)
                result = -1;
        }
        return result;
    }

    return ::kill((pid_t)pid, SIGTERM);
}
