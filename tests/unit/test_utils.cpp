// Tests for src/base/Utils.cpp

#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

#include "base/Utils.h"
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

TEST(isValidAppId_accepts_normal_ids)
{
    CHECK(Utils::isValidAppId("com.webos.app.browser"));
    CHECK(Utils::isValidAppId("org.webosports.app.phone"));
    CHECK(Utils::isValidAppId("com.example.app-2_plus+more"));
    CHECK(Utils::isValidAppId("a"));
    CHECK(Utils::isValidAppId("0com.digits.first"));
}

TEST(isValidAppId_rejects_hostile_ids)
{
    CHECK(!Utils::isValidAppId(""));
    CHECK(!Utils::isValidAppId("../../../etc/passwd"));
    CHECK(!Utils::isValidAppId("com.example/../../evil"));
    CHECK(!Utils::isValidAppId("com.example/sub"));
    CHECK(!Utils::isValidAppId("com.example.."));
    CHECK(!Utils::isValidAppId(".hidden"));
    CHECK(!Utils::isValidAppId("-dash.first"));
    CHECK(!Utils::isValidAppId("a;reboot"));
    CHECK(!Utils::isValidAppId("a b"));
    CHECK(!Utils::isValidAppId("a\"quote"));
    CHECK(!Utils::isValidAppId("a\nnewline"));
    CHECK(!Utils::isValidAppId(std::string(300, 'a')));
    CHECK(!Utils::isValidAppId(std::string("com.a\0b", 7)));
}

TEST(dir_size_counts_real_file_sizes_recursively)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());

    writeFile(dir + "/a.txt", std::string(1000, 'x'));
    CHECK(Utils::make_dir(dir + "/sub"));
    writeFile(dir + "/sub/b.txt", std::string(500, 'y'));

    long long size = Utils::dir_size(dir);
    // at least the file payload; directories may add block sizes on some
    // filesystems, but never less than the bytes written
    CHECK(size >= 1500);
    CHECK(size < 1500 + 65536);

    CHECK(Utils::remove_dir(dir));
    CHECK(!Utils::is_File_exist(dir));
}

TEST(make_dir_rejects_existing_file_and_symlink)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());

    writeFile(dir + "/file", "hello");
    CHECK(!Utils::make_dir(dir + "/file", false));

    CHECK(symlink("/tmp", (dir + "/link").c_str()) == 0);
    CHECK(!Utils::make_dir(dir + "/link", false));

    CHECK(Utils::make_dir(dir + "/newdir", false));
    // creating an existing real directory is fine
    CHECK(Utils::make_dir(dir + "/newdir", false));

    Utils::remove_dir(dir);
}

TEST(copy_file_copies_content)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());

    writeFile(dir + "/src", "payload-123");
    CHECK(Utils::copy_file(dir + "/src", dir + "/dst"));
    CHECK_EQ(Utils::read_file(dir + "/dst"), "payload-123");

    CHECK(!Utils::copy_file(dir + "/missing", dir + "/dst2"));

    Utils::remove_dir(dir);
}

TEST(file_size_and_existence)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());

    writeFile(dir + "/f", "12345");
    CHECK_EQ(Utils::file_size(dir + "/f"), 5);
    CHECK_EQ(Utils::file_size(dir + "/missing"), -1);
    CHECK(Utils::is_File_exist(dir + "/f"));
    CHECK(!Utils::is_File_exist(dir + "/missing"));

    Utils::remove_dir(dir);
}

TEST(pwa_url_helpers)
{
    CHECK(Utils::isPWA("pwa:///tmp/somewhere"));
    CHECK(!Utils::isPWA("/tmp/somewhere.ipk"));
    CHECK_EQ(Utils::getPWAPath("pwa:///tmp/somewhere"), "/tmp/somewhere");
}
