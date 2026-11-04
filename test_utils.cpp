#include "common.hpp"
#include <cerrno>

class Utils : public MycpTest {};

/* ---- write_all ---- */
CASE(Utils, Io, WriteAllWritesEverything)
{
    std::string d = pattern(10000);
    int fd = open(p("f").c_str(), O_WRONLY | O_CREAT, 0644);
    ASSERT_GE(fd, 0);
    EXPECT_EQ(0, write_all(fd, d.data(), d.size()));
    close(fd);
    EXPECT_EQ(d, slurp(p("f")));
}

CASE(Utils, Io, WriteAllZeroBytesIsNoop)
{
    EXPECT_EQ(0, write_all(-1, "x", 0));
}

CASE(Utils, Io, WriteAllRetriesAfterEintr)
{
    int fd = open(p("f").c_str(), O_WRONLY | O_CREAT, 0644);
    fi_fail_at(FI_WRITE, 1, EINTR);
    EXPECT_EQ(0, write_all(fd, "hello", 5));
    EXPECT_EQ(2, fi_calls(FI_WRITE));
    close(fd);
    EXPECT_EQ("hello", slurp(p("f")));
}

CASE(Utils, Io, WriteAllReportsHardError)
{
    int fd = open(p("f").c_str(), O_WRONLY | O_CREAT, 0644);
    fi_fail_at(FI_WRITE, 1, ENOSPC);
    EXPECT_EQ(-1, write_all(fd, "hello", 5));
    close(fd);
}

CASE(Utils, Io, WriteAllBadDescriptor)
{
    EXPECT_EQ(-1, write_all(-1, "x", 1));
}

CASE(Utils, Io, WriteAllHandlesPartialWrites)
{
    int fd = open(p("f").c_str(), O_WRONLY | O_CREAT, 0644);
    fi_io_chunk(0, 3);
    EXPECT_EQ(0, write_all(fd, "0123456789", 10));
    EXPECT_EQ(4, fi_calls(FI_WRITE));            /* 3+3+3+1 */
    close(fd);
    EXPECT_EQ("0123456789", slurp(p("f")));
}

/* ---- read_full ---- */
CASE(Utils, Io, ReadFullFillsBuffer)
{
    put(p("f"), pattern(100));
    int fd = open(p("f").c_str(), O_RDONLY);
    char buf[64];
    EXPECT_EQ(64, read_full(fd, buf, sizeof(buf)));
    EXPECT_EQ(0, memcmp(buf, pattern(100).data(), 64));
    close(fd);
}

CASE(Utils, Io, ReadFullStopsAtEof)
{
    put(p("f"), "abc");
    int fd = open(p("f").c_str(), O_RDONLY);
    char buf[16];
    EXPECT_EQ(3, read_full(fd, buf, sizeof(buf)));
    EXPECT_EQ(0, read_full(fd, buf, sizeof(buf)));
    close(fd);
}

CASE(Utils, Io, ReadFullRetriesAfterEintr)
{
    put(p("f"), "abcdef");
    int fd = open(p("f").c_str(), O_RDONLY);
    char buf[6];
    fi_fail_at(FI_READ, 1, EINTR);
    EXPECT_EQ(6, read_full(fd, buf, sizeof(buf)));
    close(fd);
}

CASE(Utils, Io, ReadFullReportsHardError)
{
    put(p("f"), "abcdef");
    int fd = open(p("f").c_str(), O_RDONLY);
    char buf[6];
    fi_fail_at(FI_READ, 1, EIO);
    EXPECT_EQ(-1, read_full(fd, buf, sizeof(buf)));
    close(fd);
}

CASE(Utils, Io, ReadFullHandlesShortReads)
{
    put(p("f"), "0123456789");
    int fd = open(p("f").c_str(), O_RDONLY);
    char buf[10];
    fi_io_chunk(4, 0);
    EXPECT_EQ(10, read_full(fd, buf, sizeof(buf)));
    EXPECT_EQ(0, memcmp(buf, "0123456789", 10));
    close(fd);
}

/* ---- parse_size ---- */
struct SizeCase { const char *text; int rc; size_t value; };

CASE(Utils, Parsing, ParseSizeTable)
{
    const size_t MAXB = MAX_BUF_SIZE;
    const SizeCase cases[] = {
        { "1",                      0, 1 },
        { "4096",                   0, 4096 },
        { "64K",                    0, 64 * 1024 },
        { "64k",                    0, 64 * 1024 },
        { "1M",                     0, 1024 * 1024 },
        { "2m",                     0, 2 * 1024 * 1024 },
        { "256M",                   0, MAXB },
        { "268435456",              0, MAXB },
        { "257M",                  -1, 0 },   /* too big after suffix      */
        { "268435457",             -1, 0 },   /* too big as plain number   */
        { "0",                     -1, 0 },   /* zero                      */
        { "0K",                    -1, 0 },
        { "",                      -1, 0 },   /* empty                     */
        { "abc",                   -1, 0 },   /* not a number              */
        { "-5",                    -1, 0 },   /* sign not allowed          */
        { " 5",                    -1, 0 },   /* leading space             */
        { "99999999999999999999999", -1, 0 }, /* strtoull overflow         */
        { "10X",                   -1, 0 },   /* unknown suffix            */
        { "10KB",                  -1, 0 },   /* suffix too long           */
        { "10 ",                   -1, 0 },   /* trailing garbage          */
    };
    for (const auto &c : cases) {
        size_t v = 12345;
        EXPECT_EQ(c.rc, parse_size(c.text, &v)) << "input: '" << c.text << "'";
        if (c.rc == 0)
            EXPECT_EQ(c.value, v) << "input: '" << c.text << "'";
        else
            EXPECT_EQ(12345u, v) << "output must stay untouched on error";
    }
}

/* ---- format_size ---- */
CASE(Utils, Parsing, FormatSizeUnits)
{
    char b[64];
    struct { unsigned long long n; const char *want; } t[] = {
        { 0,                      "0 B" },
        { 1023,                   "1023 B" },
        { 1024,                   "1.00 KB" },
        { 1536,                   "1.50 KB" },
        { 1048576ULL,             "1.00 MB" },
        { 1073741824ULL,          "1.00 GB" },
        { 1099511627776ULL,       "1.00 TB" },
        { 1ULL << 60,             "1048576.00 TB" },   /* stays at the top unit */
    };
    for (const auto &c : t) {
        format_size(c.n, b, sizeof(b));
        EXPECT_STREQ(c.want, b);
    }
}

CASE(Utils, Parsing, FormatSizeTruncatesToCapacity)
{
    char b[4];
    format_size(1536, b, sizeof(b));
    EXPECT_STREQ("1.5", b);
}

/* ---- now_seconds ---- */
CASE(Utils, TimeAndPaths, NowSecondsIsMonotonic)
{
    double a = now_seconds();
    usleep(2000);
    double b = now_seconds();
    EXPECT_GT(b, a);
}

CASE(Utils, TimeAndPaths, NowSecondsUsesScriptedClock)
{
    const double v[] = { 10.25, 11.5 };
    fi_clock_script(v, 2);
    EXPECT_DOUBLE_EQ(10.25, now_seconds());
    EXPECT_DOUBLE_EQ(11.5, now_seconds());
    EXPECT_DOUBLE_EQ(11.5, now_seconds());        /* repeats the last value */
}

/* ---- join_path ---- */
CASE(Utils, TimeAndPaths, JoinPathVariants)
{
    char b[64];
    EXPECT_EQ(0, join_path(b, sizeof(b), "a", "b"));   EXPECT_STREQ("a/b", b);
    EXPECT_EQ(0, join_path(b, sizeof(b), "a/", "b"));  EXPECT_STREQ("a/b", b);
    EXPECT_EQ(0, join_path(b, sizeof(b), "/", "b"));   EXPECT_STREQ("/b", b);
    EXPECT_EQ(0, join_path(b, sizeof(b), "", "b"));    EXPECT_STREQ("/b", b);
}

CASE(Utils, TimeAndPaths, JoinPathCapacityLimit)
{
    char b[8];
    EXPECT_EQ(0, join_path(b, 4, "a", "b"));           /* "a/b" + NUL fits  */
    EXPECT_EQ(-1, join_path(b, 3, "a", "b"));          /* one byte short    */
    EXPECT_EQ(-1, join_path(b, sizeof(b), "abcdef", "gh"));
}

CASE(Utils, TimeAndPaths, JoinPathSnprintfFailure)
{
    char b[16];
    fi_fail_at(FI_SNPRINTF, 1, EIO);
    EXPECT_EQ(-1, join_path(b, sizeof(b), "a", "b"));
}

/* ---- base_name ---- */
CASE(Utils, TimeAndPaths, BaseName)
{
    EXPECT_STREQ("c.txt", base_name("/a/b/c.txt"));
    EXPECT_STREQ("c.txt", base_name("c.txt"));
    EXPECT_STREQ("", base_name("dir/"));
    EXPECT_STREQ("", base_name("/"));
}

/* ---- one gtest test per group ---- */
GROUP(Utils, Io)
GROUP(Utils, Parsing)
GROUP(Utils, TimeAndPaths)
