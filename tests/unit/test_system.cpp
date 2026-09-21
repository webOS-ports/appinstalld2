// Tests for System::kill (no-shell process termination).

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/System.h"
#include "testfw.h"

TEST(system_kill_terminates_child)
{
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        // child: idle until killed
        for (;;)
            pause();
        _exit(0);
    }

    CHECK_EQ(System::kill((unsigned int)child, false), 0);

    int status = 0;
    CHECK_EQ(waitpid(child, &status, 0), child);
    CHECK(WIFSIGNALED(status));
    CHECK_EQ(WTERMSIG(status), SIGTERM);
}

TEST(system_kill_recursive_terminates_tree)
{
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        // child: spawn a grandchild, then idle
        pid_t grandchild = fork();
        if (grandchild == 0) {
            for (;;)
                pause();
            _exit(0);
        }
        for (;;)
            pause();
        _exit(0);
    }

    // give the child time to fork the grandchild
    usleep(200 * 1000);

    CHECK_EQ(System::kill((unsigned int)child, true), 0);

    int status = 0;
    CHECK_EQ(waitpid(child, &status, 0), child);
    CHECK(WIFSIGNALED(status));
    // the grandchild is reparented to init; we can only verify the root died
}

TEST(system_kill_invalid_pid_fails)
{
    CHECK_EQ(System::kill(0, false), -1);
}
