// Tests for AppPackage control-file parsing.

#include <fstream>
#include <stdlib.h>

#include "base/Utils.h"
#include "installer/AppPackage.h"
#include "testfw.h"

namespace {

std::string makeTempDir()
{
    char templ[] = "/tmp/appinstalld-test.XXXXXX";
    char *dir = mkdtemp(templ);
    return dir ? std::string(dir) : std::string();
}

void writeFile(const std::string &path, const std::string &content)
{
    std::ofstream f(path.c_str());
    f << content;
}

} // namespace

TEST(parseControl_reads_fields)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());
    std::string control = dir + "/control";

    writeFile(control,
              "Package: com.example.app\n"
              "Version: 1.2.3\n"
              "Architecture: all\n"
              "Installed-Size: 4242\n");

    AppPackage package;
    AppPackage::Control parsed;
    CHECK(package.parseControl(control, parsed));
    CHECK_EQ(parsed.getPackage(), "com.example.app");
    CHECK_EQ(parsed.getVersion(), "1.2.3");
    CHECK_EQ(parsed.getArchitecture(), "all");
    CHECK_EQ(parsed.getInstalledSize(), 4242u);

    Utils::remove_dir(dir);
}

TEST(parseControl_survives_malformed_installed_size)
{
    // regression: a hostile control file with a non-numeric Installed-Size
    // used to throw bad_lexical_cast and crash the daemon
    std::string dir = makeTempDir();
    CHECK(!dir.empty());
    std::string control = dir + "/control";

    writeFile(control,
              "Package: com.evil.app\n"
              "Installed-Size: enormous\n");

    AppPackage package;
    AppPackage::Control parsed;
    CHECK(package.parseControl(control, parsed));
    CHECK_EQ(parsed.getPackage(), "com.evil.app");
    CHECK_EQ(parsed.getInstalledSize(), 0u);

    writeFile(control, "Installed-Size: -5\n");
    AppPackage::Control parsedNegative;
    CHECK(package.parseControl(control, parsedNegative));
    CHECK_EQ(parsedNegative.getInstalledSize(), 0u);

    Utils::remove_dir(dir);
}

TEST(parseControl_missing_file_fails)
{
    AppPackage package;
    AppPackage::Control parsed;
    CHECK(!package.parseControl("/definitely/not/there", parsed));
}

TEST(saveInstalledSize_appends_field)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());
    std::string control = dir + "/control";

    writeFile(control, "Package: com.example.app\n");

    AppPackage package;
    package.saveInstalledSizeToControlFile(control, 1234);

    AppPackage::Control parsed;
    CHECK(package.parseControl(control, parsed));
    CHECK_EQ(parsed.getInstalledSize(), 1234u);

    Utils::remove_dir(dir);
}
