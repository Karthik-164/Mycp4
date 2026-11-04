#include "common.hpp"
#include <cerrno>

class Main : public MycpTest {
protected:
    std::string data;
    void SetUp() override
    {
        MycpTest::SetUp();
        data = pattern(20000);
        put(p("src"), data);
        fs::create_directories(p("tree/sub"));
        put(p("tree/a"), pattern(100));
        put(p("tree/sub/b"), pattern(200));
    }
};

/* ---------- usage / option parsing ---------- */
CASE(Main, Options, HelpGoesToStdout)
{
    std::string out, err;
    EXPECT_EQ(0, run({ "-h" }, &out, &err));
    EXPECT_TRUE(contains(out, "Usage: mycp [options] <source> <destination>"));
    EXPECT_TRUE(contains(out, "-B        benchmark"));
    EXPECT_EQ("", err);
}

CASE(Main, Options, UnknownOptionShowsUsageOnStderr)
{
    std::string out, err;
    EXPECT_EQ(1, run({ "-z", "a", "b" }, &out, &err));
    EXPECT_TRUE(contains(err, "Usage:"));
}

CASE(Main, Options, WrongArgumentCount)
{
    std::string err;
    EXPECT_EQ(1, run({}, nullptr, &err));
    EXPECT_TRUE(contains(err, "Usage:"));
    EXPECT_EQ(1, run({ "onlyone" }, nullptr, &err));
    EXPECT_EQ(1, run({ "a", "b", "c" }, nullptr, &err));
}

CASE(Main, Options, InvalidBufferSizes)
{
    for (const char *bad : { "abc", "0", "300M", "-1", "" }) {
        std::string err;
        EXPECT_EQ(1, run({ "-b", bad, p("src"), p("dst") }, nullptr, &err)) << bad;
        EXPECT_TRUE(contains(err, "invalid buffer size")) << bad;
    }
}

CASE(Main, Options, ValidBufferSizeWithSuffix)
{
    EXPECT_EQ(0, run({ "-b", "64K", p("src"), p("dst") }));
    EXPECT_EQ(data, slurp(p("dst")));
}

CASE(Main, Options, InvalidThreadCounts)
{
    for (const char *bad : { "abc", "0", "65", "-3", "4x" }) {
        std::string err;
        EXPECT_EQ(1, run({ "-j", bad, p("src"), p("dst") }, nullptr, &err)) << bad;
        EXPECT_TRUE(contains(err, "invalid thread count")) << bad;
    }
}

CASE(Main, Options, LogFileCannotBeOpened)
{
    std::string err;
    EXPECT_EQ(1, run({ "-l", p("no/such/dir/log"), p("src"), p("dst") }, nullptr, &err));
    EXPECT_TRUE(contains(err, "cannot open log file"));
    EXPECT_FALSE(exists(p("dst")));
}

CASE(Main, Options, LogRecordsStartAndSuccess)
{
    EXPECT_EQ(0, run({ "-l", p("log"), p("src"), p("dst") }));
    std::string log = slurp(p("log"));
    EXPECT_TRUE(contains(log, "mycp started"));
    EXPECT_TRUE(contains(log, "mycp finished: success"));
}

CASE(Main, Options, LogRecordsFailure)
{
    EXPECT_EQ(1, run({ "-l", p("log"), p("nope"), p("dst") }));
    std::string log = slurp(p("log"));
    EXPECT_TRUE(contains(log, "mycp finished: failure"));
    EXPECT_TRUE(contains(log, "ERROR"));
}

/* ---------- single file ---------- */
CASE(Main, SingleFile, CopiesSingleFileAndPrintsSummary)
{
    std::string out;
    EXPECT_EQ(0, run({ p("src"), p("dst") }, &out));
    EXPECT_EQ(data, slurp(p("dst")));
    EXPECT_TRUE(contains(out, "Copied '" + p("src") + "' -> '" + p("dst") + "'"));
    EXPECT_TRUE(contains(out, "---- Summary ----"));
    EXPECT_TRUE(contains(out, "Bytes copied : 20000"));
    EXPECT_FALSE(contains(out, "Files copied"));
}

CASE(Main, SingleFile, CopiesIntoExistingDirectory)
{
    fs::create_directories(p("outdir"));
    EXPECT_EQ(0, run({ p("src"), p("outdir") }));
    EXPECT_EQ(data, slurp(p("outdir/src")));
}

CASE(Main, SingleFile, TargetPathInDirectoryTooLong)
{
    fs::create_directories(p("outdir"));
    std::string err;
    fi_fail_at(FI_SNPRINTF, 1, EIO);
    EXPECT_EQ(1, run({ p("src"), p("outdir") }, nullptr, &err));
    EXPECT_TRUE(contains(err, "destination path too long"));
}

CASE(Main, SingleFile, MissingSource)
{
    std::string err;
    EXPECT_EQ(1, run({ p("nope"), p("dst") }, nullptr, &err));
    EXPECT_TRUE(contains(err, "cannot stat"));
}

CASE(Main, SingleFile, CopyFailureIsReportedWithoutSummary)
{
    std::string out, err;
    EXPECT_EQ(1, run({ p("src"), p("no/such/dst") }, &out, &err));
    EXPECT_TRUE(contains(err, "cannot create"));
    EXPECT_FALSE(contains(out, "Summary"));
}

CASE(Main, SingleFile, AllFlagsTogether)
{
    chmod(p("src").c_str(), 0741);
    std::string out;
    EXPECT_EQ(0, run({ "-i", "-p", "-m", "-v", "-b", "1K", p("src"), p("dst") }, &out));
    EXPECT_TRUE(contains(out, "Source file info"));
    EXPECT_TRUE(contains(out, "Destination file info"));
    EXPECT_TRUE(contains(out, "100%"));
    EXPECT_TRUE(contains(out, "Verified:"));
    EXPECT_EQ((mode_t)0741, mode_of(p("dst")));
}

CASE(Main, SingleFile, ResumeFlag)
{
    put(p("dst"), data.substr(0, 8000));
    std::string out;
    EXPECT_EQ(0, run({ "-c", p("src"), p("dst") }, &out));
    EXPECT_TRUE(contains(out, "Resuming"));
    EXPECT_EQ(data, slurp(p("dst")));
}

CASE(Main, SingleFile, SummaryHandlesZeroElapsedTime)
{
    const double v[] = { 5.0, 5.0 };              /* copy_file start == end */
    fi_clock_script(v, 2);
    std::string out;
    EXPECT_EQ(0, run({ p("src"), p("dst") }, &out));
    EXPECT_TRUE(contains(out, "Time taken   : 0.0000 s"));
    EXPECT_TRUE(contains(out, "Speed        : 0.00 MB/s"));
}

CASE(Main, SingleFile, SummaryComputesSpeed)
{
    const double v[] = { 5.0, 6.0 };              /* exactly one second */
    fi_clock_script(v, 2);
    std::string out;
    EXPECT_EQ(0, run({ p("src"), p("dst") }, &out));
    EXPECT_TRUE(contains(out, "Time taken   : 1.0000 s"));
    EXPECT_TRUE(contains(out, "Speed        : 0.02 MB/s"));
}

/* ---------- directories ---------- */
CASE(Main, DirectoryAndBenchmark, DirectoryWithoutRecursiveFlag)
{
    std::string err;
    EXPECT_EQ(1, run({ p("tree"), p("out") }, nullptr, &err));
    EXPECT_TRUE(contains(err, "use -r"));
    EXPECT_FALSE(exists(p("out")));
}

CASE(Main, DirectoryAndBenchmark, RecursiveCopyPrintsDirectorySummary)
{
    std::string out;
    EXPECT_EQ(0, run({ "-r", p("tree"), p("out") }, &out));
    EXPECT_EQ(pattern(200), slurp(p("out/sub/b")));
    EXPECT_TRUE(contains(out, "Files copied : 2"));
    EXPECT_TRUE(contains(out, "Files failed : 0"));
}

CASE(Main, DirectoryAndBenchmark, RecursiveFailureReturnsOne)
{
    std::string out;
    fi_fail_at(FI_OPEN, 1, EIO);
    EXPECT_EQ(1, run({ "-r", p("tree"), p("out") }, &out));
    EXPECT_TRUE(contains(out, "Files failed : 1"));
}

CASE(Main, DirectoryAndBenchmark, ThreadsDisableProgressAndInfo)
{
    std::string out;
    EXPECT_EQ(0, run({ "-r", "-j", "4", "-p", p("tree"), p("out") }, &out));
    EXPECT_TRUE(contains(out, "disabled with multiple threads"));
    EXPECT_FALSE(contains(out, "100%"));
}

CASE(Main, DirectoryAndBenchmark, ThreadsDisableInfoOnly)
{
    std::string out;
    EXPECT_EQ(0, run({ "-r", "-j", "2", "-i", p("tree"), p("out") }, &out));
    EXPECT_TRUE(contains(out, "disabled with multiple threads"));
    EXPECT_FALSE(contains(out, "---- Source file info"));
}

CASE(Main, DirectoryAndBenchmark, ThreadsWithoutDisplayOptionsPrintNoNote)
{
    std::string out;
    EXPECT_EQ(0, run({ "-r", "-j", "3", p("tree"), p("out") }, &out));
    EXPECT_FALSE(contains(out, "disabled with multiple threads"));
}

CASE(Main, DirectoryAndBenchmark, SingleThreadKeepsProgressAndInfo)
{
    std::string out;
    EXPECT_EQ(0, run({ "-r", "-j", "1", "-p", "-i", p("tree"), p("out") }, &out));
    EXPECT_FALSE(contains(out, "disabled with multiple threads"));
    EXPECT_TRUE(contains(out, "file info"));
    EXPECT_TRUE(contains(out, "100%"));
}

/* ---------- benchmark ---------- */
CASE(Main, DirectoryAndBenchmark, BenchmarkOnRegularFile)
{
    std::string out;
    EXPECT_EQ(0, run({ "-B", p("src"), p("bench_dst") }, &out));
    EXPECT_TRUE(contains(out, "Fastest buffer size:"));
    EXPECT_FALSE(exists(p("bench_dst")));
}

CASE(Main, DirectoryAndBenchmark, BenchmarkRejectsDirectory)
{
    std::string err;
    EXPECT_EQ(1, run({ "-B", p("tree"), p("x") }, nullptr, &err));
    EXPECT_TRUE(contains(err, "benchmark needs a regular file"));
}

/* ---- one gtest test per group ---- */
GROUP(Main, Options)
GROUP(Main, SingleFile)
GROUP(Main, DirectoryAndBenchmark)
