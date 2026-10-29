// Tests for src/installer/PermissionFilter.cpp and src/installer/RoleGenerator.cpp:
// what an application installed by appinstalld may hold, and at which trust
// level its role is written.

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "installer/PermissionFilter.h"
#include "installer/RoleGenerator.h"
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

std::string readFile(const std::string &path)
{
    std::ifstream f(path.c_str());
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool contains(const std::vector<std::string> &v, const std::string &s)
{
    for (const auto &x : v)
        if (x == s)
            return true;
    return false;
}

} // namespace

TEST(alwaysDenied_matches_privileged_groups)
{
    using PermissionFilter::isAlwaysDenied;
    CHECK(isAlwaysDenied("all"));
    CHECK(isAlwaysDenied("private"));
    CHECK(isAlwaysDenied("applicationinstall.management"));
    CHECK(isAlwaysDenied("applicationinstall.devmode"));
    CHECK(isAlwaysDenied("appmanager.management"));
    CHECK(isAlwaysDenied("database.management"));
    CHECK(isAlwaysDenied("auth.passcode"));
    CHECK(isAlwaysDenied("auth.query"));
    CHECK(isAlwaysDenied("devmode-service.operation"));
    CHECK(isAlwaysDenied("arescli.interface"));
    CHECK(isAlwaysDenied("settings.devutility"));
    CHECK(isAlwaysDenied("imlibpurple-call.debug"));
    CHECK(isAlwaysDenied("networking.internal"));
    CHECK(isAlwaysDenied("database.profiling"));
}

TEST(alwaysDenied_leaves_ordinary_groups)
{
    using PermissionFilter::isAlwaysDenied;
    CHECK(!isAlwaysDenied("public"));
    CHECK(!isAlwaysDenied("notification.operation"));
    CHECK(!isAlwaysDenied("application.operation"));
    CHECK(!isAlwaysDenied("time.query"));
    // substrings that only look like a denied pattern
    CHECK(!isAlwaysDenied("allowed.operation"));
    CHECK(!isAlwaysDenied("management.query"));
    CHECK(!isAlwaysDenied("oauth.operation"));
    CHECK(!isAlwaysDenied(""));
}

TEST(filter_keeps_only_allowlisted_groups)
{
    const std::set<std::string> allow = { "public", "notification.operation", "time.query" };
    std::vector<std::string> dropped;
    auto kept = PermissionFilter::filter(
        { "notification.operation", "ipkg-service.operation", "time.query", "wifi.query" },
        allow, &dropped);

    CHECK_EQ(kept.size(), 2u);
    CHECK(contains(kept, "notification.operation"));
    CHECK(contains(kept, "time.query"));
    CHECK(contains(dropped, "ipkg-service.operation"));
    CHECK(contains(dropped, "wifi.query"));
}

TEST(filter_strips_denied_groups_even_if_allowlisted)
{
    // a mistaken allowlist must not hand out "all" or a management group
    const std::set<std::string> allow = { "all", "private", "appmanager.management",
                                          "auth.passcode", "time.query" };
    std::vector<std::string> dropped;
    auto kept = PermissionFilter::filter(
        { "all", "private", "appmanager.management", "auth.passcode", "time.query" },
        allow, &dropped);

    CHECK_EQ(kept.size(), 1u);
    CHECK(contains(kept, "time.query"));
    CHECK(!contains(kept, "all"));
    CHECK_EQ(dropped.size(), 4u);
}

TEST(filter_empty_allowlist_grants_nothing)
{
    auto kept = PermissionFilter::filter({ "public", "time.query" }, {}, nullptr);
    CHECK(kept.empty());
}

TEST(filter_keeps_order_and_drops_duplicates)
{
    const std::set<std::string> allow = { "a.query", "b.query" };
    auto kept = PermissionFilter::filter({ "b.query", "a.query", "b.query" }, allow);
    CHECK_EQ(kept.size(), 2u);
    CHECK(kept.size() == 2 && kept[0] == "b.query" && kept[1] == "a.query");
}

TEST(loadAllowlist_reads_grantable)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());
    std::string path = dir + "/third-party-grantable.json";
    writeFile(path, "{\"version\": 1, \"grantable\": [\"public\", \"time.query\"]}");

    std::set<std::string> out;
    CHECK(PermissionFilter::loadAllowlist(path, out));
    CHECK_EQ(out.size(), 2u);
    CHECK(out.count("time.query") == 1);

    unlink(path.c_str());
    rmdir(dir.c_str());
}

TEST(loadAllowlist_fails_closed)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());
    std::string path = dir + "/a.json";
    std::set<std::string> out = { "stale" };

    // missing file
    CHECK(!PermissionFilter::loadAllowlist(dir + "/missing.json", out));
    CHECK(out.empty());

    // not JSON
    writeFile(path, "grantable: all");
    out = { "stale" };
    CHECK(!PermissionFilter::loadAllowlist(path, out));
    CHECK(out.empty());

    // wrong shape
    writeFile(path, "{\"grantable\": \"all\"}");
    CHECK(!PermissionFilter::loadAllowlist(path, out));
    CHECK(out.empty());

    // a non-string entry rejects the whole file
    writeFile(path, "{\"grantable\": [\"time.query\", 7]}");
    CHECK(!PermissionFilter::loadAllowlist(path, out));
    CHECK(out.empty());

    unlink(path.c_str());
    rmdir(dir.c_str());
}

TEST(roleGenerator_writes_dev_trust_level)
{
    CHECK(std::string(PermissionFilter::kInstalledAppTrustLevel) == "dev");

    std::string dir = makeTempDir();
    CHECK(!dir.empty());
    std::string tmpl = dir + "/WebApp.json";
    std::string out = dir + "/role.json";
    writeFile(tmpl,
              "{\n"
              "\t\"appId\": \"XXXIDXXX\",\n"
              "\t\"exeName\": \"XXXEXEPATHXXX\",\n"
              "\t\"trustLevel\": \"XXXPERMLEVELXXX\"\n"
              "}\n");

    CHECK(RoleGenerator::generate(tmpl, out, "com.example.app", "/media/cryptofs/apps/x/bin"));
    std::string role = readFile(out);
    CHECK(role.find("\"trustLevel\": \"dev\"") != std::string::npos);
    CHECK(role.find("oem") == std::string::npos);
    CHECK(role.find("\"appId\": \"com.example.app\"") != std::string::npos);
    CHECK(role.find("XXX") == std::string::npos);

    unlink(tmpl.c_str());
    unlink(out.c_str());
    rmdir(dir.c_str());
}

TEST(roleGenerator_missing_template_fails)
{
    std::string dir = makeTempDir();
    CHECK(!dir.empty());
    CHECK(!RoleGenerator::generate(dir + "/none.json", dir + "/role.json", "a", "b"));
    CHECK(access((dir + "/role.json").c_str(), F_OK) != 0);
    rmdir(dir.c_str());
}

TEST(shipped_allowlist_has_no_denied_group)
{
    // APPINSTALLD_ALLOWLIST points at the generated third-party-grantable.json
    // when the test is run from the build; skipped otherwise.
    const char *path = std::getenv("APPINSTALLD_ALLOWLIST");
    if (!path)
        return;
    std::set<std::string> allow;
    CHECK(PermissionFilter::loadAllowlist(path, allow));
    CHECK(!allow.empty());
    for (const auto &g : allow)
        CHECK(!PermissionFilter::isAlwaysDenied(g));
}

TEST(requestedGroups_accepts_only_string_arrays)
{
    std::vector<std::string> out;

    pbnjson::JValue good = pbnjson::Array();
    good << "time.query" << "all";
    CHECK(PermissionFilter::requestedGroups(good, out));
    CHECK_EQ(out.size(), 2u);

    out = { "stale" };
    CHECK(!PermissionFilter::requestedGroups(pbnjson::JValue(), out));
    CHECK(out.empty());

    CHECK(!PermissionFilter::requestedGroups(pbnjson::JValue("all"), out));

    pbnjson::JValue mixed = pbnjson::Array();
    mixed << "time.query" << 3;
    out = { "stale" };
    CHECK(!PermissionFilter::requestedGroups(mixed, out));
    CHECK(out.empty());
}

TEST(grantedGroups_falls_back_to_public)
{
    const std::set<std::string> allow = { "time.query" };
    std::vector<std::string> dropped;

    auto granted = PermissionFilter::grantedGroups({ "all", "ipkg-service.operation" }, allow, &dropped);
    CHECK_EQ(granted.size(), 1u);
    CHECK(granted.size() == 1 && granted[0] == "public");
    CHECK_EQ(dropped.size(), 2u);

    granted = PermissionFilter::grantedGroups({ "time.query", "all" }, allow);
    CHECK_EQ(granted.size(), 1u);
    CHECK(granted.size() == 1 && granted[0] == "time.query");
}

TEST(appinfo_requesting_all_gets_nothing_privileged)
{
    // what appinstalld does with the appinfo.json of a hostile app
    std::string dir = makeTempDir();
    CHECK(!dir.empty());
    std::string path = dir + "/third-party-grantable.json";
    writeFile(path, "{\"grantable\": [\"public\", \"notification.operation\"]}");
    std::set<std::string> allow;
    CHECK(PermissionFilter::loadAllowlist(path, allow));

    pbnjson::JValue appinfo = pbnjson::JDomParser::fromString(
        "{\"id\": \"com.evil.app\", \"requiredPermissions\": "
        "[\"all\", \"applicationinstall.management\", \"ipkg-service.operation\", "
        "\"auth.passcode\", \"notification.operation\"]}");
    std::vector<std::string> requested;
    CHECK(PermissionFilter::requestedGroups(appinfo["requiredPermissions"], requested));
    auto granted = PermissionFilter::grantedGroups(requested, allow);
    CHECK_EQ(granted.size(), 1u);
    CHECK(granted.size() == 1 && granted[0] == "notification.operation");

    unlink(path.c_str());
    rmdir(dir.c_str());
}
