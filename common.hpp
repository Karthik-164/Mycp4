// common.hpp - shared helpers for the mycp unit tests
#pragma once

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <map>
#include <utility>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>

extern "C" {
#include "fault_ctl.h"
#include "mycp.h"
#include "utils.h"
#include "logger.h"
#include "verify.h"
#include "display.h"
#include "copy.h"
#include "dircopy.h"
#include "benchmark.h"
int mycp_main(int argc, char *argv[]);   /* main() renamed with -Dmain=mycp_main */
}

namespace fs = std::filesystem;

/* Captures stdout + stderr (C stdio level) for the lifetime of the object. */
class Capture {
public:
    Capture()
    {
        fflush(stdout);
        fflush(stderr);
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
    }
    ~Capture() { finish(); }
    const std::string &out() { finish(); return out_; }
    const std::string &err() { finish(); return err_; }

private:
    void finish()
    {
        if (done_)
            return;
        fflush(stdout);
        fflush(stderr);
        out_ = testing::internal::GetCapturedStdout();
        err_ = testing::internal::GetCapturedStderr();
        done_ = true;
    }
    bool done_ = false;
    std::string out_, err_;
};

/* Every test gets its own temp directory and a clean fault-injection state. */
class MycpTest : public ::testing::Test {
protected:
    std::string dir;

    void SetUp() override
    {
        char tmpl[] = "/tmp/mycp_ut_XXXXXX";
        ASSERT_NE(mkdtemp(tmpl), nullptr);
        dir = tmpl;
        fi_reset();
        log_close();
    }
    void TearDown() override
    {
        fi_reset();
        log_close();
        std::error_code ec;
        fs::permissions(dir, fs::perms::owner_all, ec);
        fs::remove_all(dir, ec);
    }

    std::string p(const std::string &name) const { return dir + "/" + name; }

    static std::string pattern(size_t n)
    {
        std::string s(n, '\0');
        for (size_t i = 0; i < n; i++)
            s[i] = static_cast<char>('a' + (i * 7 + i / 13) % 26);
        return s;
    }
    static void put(const std::string &path, const std::string &data,
                    mode_t mode = 0644)
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f << data;
        f.close();
        chmod(path.c_str(), mode);
    }
    static std::string slurp(const std::string &path)
    {
        std::ifstream f(path, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }
    static bool exists(const std::string &path)
    {
        struct stat st;
        return lstat(path.c_str(), &st) == 0;
    }
    static mode_t mode_of(const std::string &path)
    {
        struct stat st;
        return stat(path.c_str(), &st) == 0 ? (st.st_mode & 07777) : 0;
    }
    static options_t opts(size_t buf = 4096)
    {
        options_t o;
        memset(&o, 0, sizeof(o));
        o.buf_size = buf;
        o.threads = 1;
        return o;
    }
    static bool contains(const std::string &hay, const std::string &needle)
    {
        return hay.find(needle) != std::string::npos;
    }

    /* copy_file with output captured */
    int cp(const std::string &src, const std::string &dst, const options_t &o,
           stats_t *st = nullptr, std::string *out = nullptr,
           std::string *err = nullptr)
    {
        Capture c;
        int rc = copy_file(src.c_str(), dst.c_str(), &o, st);
        if (out) *out = c.out();
        if (err) *err = c.err();
        return rc;
    }

    /* run the real main() with the given arguments */
    int run(std::vector<std::string> args, std::string *out = nullptr,
            std::string *err = nullptr)
    {
        std::vector<char *> argv;
        std::string prog = "mycp";
        argv.push_back(prog.data());
        for (auto &a : args)
            argv.push_back(a.data());
        argv.push_back(nullptr);
        optind = 0;                       /* force getopt to restart */
        Capture c;
        int rc = mycp_main(static_cast<int>(args.size()) + 1, argv.data());
        if (out) *out = c.out();
        if (err) *err = c.err();
        return rc;
    }
};


/* ------------------------------------------------------------------------
 * CASE(Suite, Group, Name) defines a self-contained sub-case (own fixture:
 * fresh temp dir, clean fault-injection state).  GROUP(Suite, Group) turns all
 * cases registered for that group into ONE gtest test; SCOPED_TRACE tells
 * which sub-case failed.
 * ---------------------------------------------------------------------- */
using CaseFn = void (*)();
inline std::map<std::string, std::vector<std::pair<std::string, CaseFn>>> &case_registry()
{
    static std::map<std::string, std::vector<std::pair<std::string, CaseFn>>> r;
    return r;
}
struct CaseRegistrar {
    CaseRegistrar(const char *group, const char *name, CaseFn fn)
    {
        case_registry()[group].emplace_back(name, fn);
    }
};
inline void run_group(const char *group)
{
    auto &cases = case_registry()[group];
    EXPECT_FALSE(cases.empty()) << "no cases registered for " << group;
    for (auto &c : cases) {
        SCOPED_TRACE(std::string("sub-case: ") + group + " / " + c.first);
        c.second();
    }
}

#define CASE(Suite, Group, Name)                                              \
    struct Suite##_##Name : Suite {                                           \
        void TestBody() override {}                                           \
        void body();                                                          \
        static void case_run()                                                   \
        {                                                                     \
            Suite##_##Name c;                                                 \
            c.SetUp();                                                        \
            c.body();                                                         \
            c.TearDown();                                                     \
        }                                                                     \
    };                                                                        \
    static CaseRegistrar reg_##Suite##_##Name(#Suite "." #Group, #Name,       \
                                              &Suite##_##Name::case_run);          \
    void Suite##_##Name::body()

#define GROUP(Suite, Group) \
    TEST(Suite, Group) { run_group(#Suite "." #Group); }
