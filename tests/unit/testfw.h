// Minimal assert-based test framework for appinstalld2 unit tests.
// No external dependencies so it cross-compiles for every target.

#ifndef TESTFW_H
#define TESTFW_H

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace testfw {

struct Registry {
    struct Case { std::string name; std::function<void()> fn; };
    std::vector<Case> cases;
    int failures = 0;
    std::string current;

    static Registry& instance()
    {
        static Registry r;
        return r;
    }

    static int runAll()
    {
        Registry& r = instance();
        for (auto &c : r.cases) {
            r.current = c.name;
            std::printf("[ RUN  ] %s\n", c.name.c_str());
            int before = r.failures;
            c.fn();
            std::printf("[ %s ] %s\n", r.failures == before ? " OK " : "FAIL", c.name.c_str());
        }
        std::printf("== %zu tests, %d failed assertions ==\n", r.cases.size(), r.failures);
        return r.failures == 0 ? 0 : 1;
    }
};

struct Registrar {
    Registrar(const char *name, std::function<void()> fn)
    {
        Registry::instance().cases.push_back({name, std::move(fn)});
    }
};

} // namespace testfw

#define TEST(name) \
    static void test_##name(); \
    static testfw::Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            ++testfw::Registry::instance().failures; \
            std::printf("  CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define CHECK_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            ++testfw::Registry::instance().failures; \
            std::printf("  CHECK_EQ failed at %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); \
        } \
    } while (0)

#endif // TESTFW_H
