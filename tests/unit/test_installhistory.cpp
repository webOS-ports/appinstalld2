// Tests for the TaskStep <-> string mapping in InstallHistory.h.

#include "installer/InstallHistory.h"
#include "testfw.h"

TEST(taskstep_roundtrip)
{
    TaskStepParser parser;
    const TaskStep steps[] = {
        Unknown, IpkInstallNeeded, IpkInstallComplete, IpkRemoveNeeded,
        InstallComplete, RemoveComplete, AppCloseNeeded, IpkParseNeeded,
        ServiceInstallNeeded, ServiceUninstallComplete, DataRemoveNeeded,
        RemoveJailComplete, InstallSmackNeeded, RemoveSmackComplete,
        ErrorInstall, ErrorRemove, Finish,
    };

    for (TaskStep step : steps) {
        std::string name = parser.enumToStringStep(step);
        CHECK(!name.empty());
        CHECK_EQ(parser.stringToEnumStep(name), step);
    }
}

TEST(taskstep_unknown_string)
{
    TaskStepParser parser;
    CHECK_EQ(parser.stringToEnumStep("NoSuchStepName"), Undefied);
    CHECK_EQ(parser.stringToEnumStep(""), Undefied);
}
