#include "common.hpp"
#include <cerrno>
#include <regex>

class Logger : public MycpTest {};

CASE(Logger, Basics, NoLogOpenIsSilentNoop)
{
    Capture c;
    log_info("nothing %d", 1);
    EXPECT_EQ("", c.out());
    EXPECT_EQ("", c.err());
}

CASE(Logger, Basics, ReportErrorPrintsEvenWithoutLog)
{
    Capture c;
    report_error("boom %s", "now");
    EXPECT_EQ("mycp: boom now\n", c.err());
}

CASE(Logger, Basics, OpenWritesTimestampedInfoLine)
{
    ASSERT_EQ(0, log_open(p("log").c_str()));
    log_info("hello %d", 42);
    log_close();
    std::string s = slurp(p("log"));
    std::regex re(R"(^\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\] INFO  hello 42\n$)");
    EXPECT_TRUE(std::regex_match(s, re)) << s;
}

CASE(Logger, Basics, ReportErrorGoesToStderrAndLog)
{
    ASSERT_EQ(0, log_open(p("log").c_str()));
    Capture c;
    report_error("bad thing: %s", "x");
    EXPECT_EQ("mycp: bad thing: x\n", c.err());
    log_close();
    std::string s = slurp(p("log"));
    EXPECT_TRUE(contains(s, "ERROR bad thing: x\n")) << s;
}

CASE(Logger, Basics, AppendsToExistingFile)
{
    put(p("log"), "old line\n");
    ASSERT_EQ(0, log_open(p("log").c_str()));
    log_info("new");
    log_close();
    std::string s = slurp(p("log"));
    EXPECT_EQ(0u, s.find("old line\n"));
    EXPECT_TRUE(contains(s, "INFO  new"));
}

CASE(Logger, Basics, OpenFailureIsReported)
{
    Capture c;
    EXPECT_EQ(-1, log_open(p("no/such/dir/log").c_str()));
    EXPECT_TRUE(contains(c.err(), "cannot open log file"));
    log_info("still harmless");               /* g_fd stays -1 */
}

CASE(Logger, Basics, CloseIsIdempotent)
{
    ASSERT_EQ(0, log_open(p("log").c_str()));
    log_close();
    log_close();
    log_info("after close");
    EXPECT_EQ("", slurp(p("log")));
}

CASE(Logger, EdgeCases, OverlongMessageIsTruncated)
{
    ASSERT_EQ(0, log_open(p("log").c_str()));
    std::string big(3000, 'x');
    log_info("%s", big.c_str());
    log_close();
    std::string s = slurp(p("log"));
    EXPECT_EQ('\n', s.back());
    EXPECT_LT(s.size(), 1700u);
    EXPECT_GT(s.size(), 1500u);
}

CASE(Logger, EdgeCases, SnprintfErrorDropsTheLine)
{
    ASSERT_EQ(0, log_open(p("log").c_str()));
    fi_fail_at(FI_SNPRINTF, 1, EIO);
    log_info("lost");
    log_close();
    EXPECT_EQ("", slurp(p("log")));
}

CASE(Logger, EdgeCases, TruncatedLineStillEndsWithNewline)
{
    ASSERT_EQ(0, log_open(p("log").c_str()));
    fi_snprintf_truncate(5000);               /* snprintf reports "did not fit" */
    fi_fail_at(FI_SNPRINTF, 1, EIO);
    log_info("whatever");
    log_close();
    std::string s = slurp(p("log"));
    EXPECT_EQ(2047u, s.size());
    EXPECT_EQ('\n', s.back());
}

/* ---- one gtest test per group ---- */
GROUP(Logger, Basics)
GROUP(Logger, EdgeCases)
