#include "common.hpp"
#include <cerrno>

class DirCopy : public MycpTest {
protected:
    std::string src, dst;

    void SetUp() override
    {
        MycpTest::SetUp();
        src = p("src");
        dst = p("dst");
        fs::create_directories(src + "/sub/deep");
        fs::create_directories(src + "/empty");
        put(src + "/a.txt", pattern(5000));
        put(src + "/sub/b.txt", pattern(300));
        put(src + "/sub/deep/c.txt", pattern(70000));
    }

    int cpdir(const options_t &o, stats_t *st = nullptr, std::string *out = nullptr,
              std::string *err = nullptr, const std::string &from = "",
              const std::string &to = "")
    {
        Capture c;
        int rc = copy_directory((from.empty() ? src : from).c_str(),
                                (to.empty() ? dst : to).c_str(), &o, st);
        if (out) *out = c.out();
        if (err) *err = c.err();
        return rc;
    }

    void expect_tree_copied()
    {
        EXPECT_EQ(pattern(5000), slurp(dst + "/a.txt"));
        EXPECT_EQ(pattern(300), slurp(dst + "/sub/b.txt"));
        EXPECT_EQ(pattern(70000), slurp(dst + "/sub/deep/c.txt"));
        EXPECT_TRUE(fs::is_directory(dst + "/empty"));
    }

    void make_many(int n)
    {
        fs::create_directories(p("many"));
        for (int i = 0; i < n; i++)
            put(p("many/f" + std::to_string(i)), pattern(200 + i));
    }
};

/* ---------- successful copies ---------- */
CASE(DirCopy, Success, SingleThread)
{
    stats_t st = {};
    EXPECT_EQ(0, cpdir(opts(), &st));
    expect_tree_copied();
    EXPECT_EQ(3u, st.files_ok);
    EXPECT_EQ(0u, st.files_failed);
    EXPECT_EQ(75300u, st.bytes);
}

CASE(DirCopy, Success, StatsPointerMayBeNull)
{
    EXPECT_EQ(0, cpdir(opts(), nullptr));
    expect_tree_copied();
}

CASE(DirCopy, Success, PrintsOneLinePerFile)
{
    std::string out;
    EXPECT_EQ(0, cpdir(opts(), nullptr, &out));
    EXPECT_TRUE(contains(out, "Copied: " + src + "/a.txt -> " + dst + "/a.txt (5000 bytes)"));
}

CASE(DirCopy, Success, MultipleThreads)
{
    options_t o = opts();
    o.threads = 4;
    stats_t st = {};
    EXPECT_EQ(0, cpdir(o, &st));
    expect_tree_copied();
    EXPECT_EQ(3u, st.files_ok);
}

CASE(DirCopy, Success, ManyFilesManyThreads)
{
    make_many(40);
    options_t o = opts();
    o.threads = 8;
    stats_t st = {};
    EXPECT_EQ(0, cpdir(o, &st, nullptr, nullptr, p("many"), p("many_out")));
    EXPECT_EQ(40u, st.files_ok);
    for (int i = 0; i < 40; i++)
        EXPECT_EQ(pattern(200 + i), slurp(p("many_out/f" + std::to_string(i))));
}

CASE(DirCopy, Success, ThreadCountBelowOneBehavesLikeOne)
{
    options_t o = opts();
    o.threads = 0;
    stats_t st = {};
    EXPECT_EQ(0, cpdir(o, &st));
    EXPECT_EQ(3u, st.files_ok);
}

CASE(DirCopy, Success, ThreadCountIsCappedAtMaxThreads)
{
    make_many(MAX_THREADS + 6);
    options_t o = opts();
    o.threads = 500;
    stats_t st = {};
    EXPECT_EQ(0, cpdir(o, &st, nullptr, nullptr, p("many"), p("many_out")));
    EXPECT_EQ((unsigned long)(MAX_THREADS + 6), st.files_ok);
}

CASE(DirCopy, Success, NeverMoreThreadsThanFiles)
{
    options_t o = opts();
    o.threads = 32;
    stats_t st = {};
    EXPECT_EQ(0, cpdir(o, &st));                 /* 3 files, 32 requested */
    EXPECT_EQ(3u, st.files_ok);
}

CASE(DirCopy, Success, SingleFileWithManyThreadsRunsInline)
{
    fs::create_directories(p("one"));
    put(p("one/only"), "x");
    options_t o = opts();
    o.threads = 8;
    stats_t st = {};
    EXPECT_EQ(0, cpdir(o, &st, nullptr, nullptr, p("one"), p("one_out")));
    EXPECT_EQ(1u, st.files_ok);
}

CASE(DirCopy, Success, EmptySourceDirectory)
{
    fs::create_directories(p("void"));
    options_t o = opts();
    o.threads = 4;
    stats_t st = {};
    EXPECT_EQ(0, cpdir(o, &st, nullptr, nullptr, p("void"), p("void_out")));
    EXPECT_EQ(0u, st.files_ok);
    EXPECT_TRUE(fs::is_directory(p("void_out")));
}

CASE(DirCopy, Success, ExistingDestinationDirectoryIsReused)
{
    fs::create_directories(dst);
    put(dst + "/keep.txt", "keep");
    EXPECT_EQ(0, cpdir(opts()));
    expect_tree_copied();
    EXPECT_EQ("keep", slurp(dst + "/keep.txt"));
}

CASE(DirCopy, Success, SymlinksAndFifosAreSkipped)
{
    ASSERT_EQ(0, symlink(src.c_str(), (src + "/link").c_str()));
    ASSERT_EQ(0, mkfifo((src + "/pipe").c_str(), 0644));
    ASSERT_EQ(0, log_open(p("log").c_str()));
    stats_t st = {};
    EXPECT_EQ(0, cpdir(opts(), &st));
    log_close();
    EXPECT_FALSE(exists(dst + "/link"));
    EXPECT_FALSE(exists(dst + "/pipe"));
    EXPECT_EQ(3u, st.files_ok);
    EXPECT_TRUE(contains(slurp(p("log")), "skipping"));
}

CASE(DirCopy, Success, DestinationInsideSourceIsNotRecursed)
{
    std::string inner = src + "/out";
    stats_t st = {};
    EXPECT_EQ(0, cpdir(opts(), &st, nullptr, nullptr, src, inner));
    EXPECT_EQ(3u, st.files_ok);
    EXPECT_FALSE(exists(inner + "/out"));
    EXPECT_EQ(pattern(5000), slurp(inner + "/a.txt"));
}

CASE(DirCopy, Success, EntryOnAnotherDeviceIsNotSkipped)
{
    fi_lstat_flip_dev_at(1);        /* first entry looks like it is on another fs */
    stats_t st = {};
    EXPECT_EQ(0, cpdir(opts(), &st));
    EXPECT_EQ(3u, st.files_ok);
}

/* ---------- permissions ---------- */
CASE(DirCopy, Permissions, PreserveRestoresDirectoryAndFileModes)
{
    chmod(src.c_str(), 0750);
    chmod((src + "/sub").c_str(), 0711);
    chmod((src + "/sub/deep").c_str(), 0500);
    chmod((src + "/a.txt").c_str(), 0640);
    options_t o = opts();
    o.preserve = 1;
    EXPECT_EQ(0, cpdir(o));
    EXPECT_EQ((mode_t)0750, mode_of(dst));
    EXPECT_EQ((mode_t)0711, mode_of(dst + "/sub"));
    EXPECT_EQ((mode_t)0500, mode_of(dst + "/sub/deep"));
    EXPECT_EQ((mode_t)0640, mode_of(dst + "/a.txt"));
    chmod((src + "/sub/deep").c_str(), 0755);       /* let TearDown clean up */
    chmod((dst + "/sub/deep").c_str(), 0755);
}

CASE(DirCopy, Permissions, PreserveChmodFailureIsReportedNotFatal)
{
    options_t o = opts();
    o.preserve = 1;
    std::string err;
    fi_fail_at(FI_CHMOD, 1, EPERM);
    EXPECT_EQ(0, cpdir(o, nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "cannot set permissions"));
}

CASE(DirCopy, Permissions, PreserveAllocationFailureForDirList)
{
    options_t o = opts();
    o.preserve = 1;
    std::string err;
    fi_fail_at(FI_MALLOC, 1, ENOMEM);               /* add_dir's malloc */
    EXPECT_EQ(-1, cpdir(o, nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "out of memory"));
}

CASE(DirCopy, Permissions, PreserveStrdupFailureForDirList)
{
    options_t o = opts();
    o.preserve = 1;
    std::string err;
    fi_fail_at(FI_STRDUP, 1, ENOMEM);               /* add_dir's strdup */
    EXPECT_EQ(-1, cpdir(o, nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "out of memory"));
}

/* ---------- allocation failures while queueing ---------- */
CASE(DirCopy, Errors, QueueNodeAllocationFails)
{
    std::string err;
    stats_t st = {};
    fi_fail_at(FI_MALLOC, 1, ENOMEM);               /* first push_job malloc */
    EXPECT_EQ(-1, cpdir(opts(), &st, nullptr, &err));
    EXPECT_TRUE(contains(err, "out of memory"));
    EXPECT_EQ(2u, st.files_ok);                     /* the other files still copy */
}

CASE(DirCopy, Errors, QueueSourceStrdupFails)
{
    std::string err;
    fi_fail_at(FI_STRDUP, 1, ENOMEM);
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "out of memory"));
}

CASE(DirCopy, Errors, QueueDestinationStrdupFails)
{
    std::string err;
    fi_fail_at(FI_STRDUP, 2, ENOMEM);
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "out of memory"));
}

/* ---------- walk errors ---------- */
CASE(DirCopy, Errors, MissingSource)
{
    std::string err;
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err, p("nope"), p("out")));
    EXPECT_TRUE(contains(err, "cannot stat"));
}

CASE(DirCopy, Errors, DestinationParentMissing)
{
    std::string err;
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err, src, p("a/b/c")));
    EXPECT_TRUE(contains(err, "cannot create directory"));
}

CASE(DirCopy, Errors, DestinationIsARegularFile)
{
    put(dst, "i am a file");
    std::string err;
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "is not a directory"));
}

CASE(DirCopy, Errors, DestinationStatFailsAfterMkdir)
{
    std::string err;
    fi_fail_at(FI_STAT, 2, EIO);                    /* 1 = stat(src), 2 = stat(dst) */
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "is not a directory"));
}

CASE(DirCopy, Errors, OpendirFailureOnTopLevel)
{
    std::string err;
    fi_fail_at(FI_OPENDIR, 1, EACCES);
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "cannot open directory"));
}

CASE(DirCopy, Errors, OpendirFailureInSubdirectoryKeepsGoing)
{
    std::string err;
    stats_t st = {};
    fi_fail_at(FI_OPENDIR, 2, EACCES);              /* some subdirectory */
    EXPECT_EQ(-1, cpdir(opts(), &st, nullptr, &err));
    EXPECT_TRUE(contains(err, "cannot open directory"));
    EXPECT_GE(st.files_ok, 1u);
}

CASE(DirCopy, Errors, LstatFailureOnEntry)
{
    std::string err;
    fi_fail_at(FI_LSTAT, 1, EIO);
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "cannot stat"));
}

CASE(DirCopy, Errors, SourcePathTooLong)
{
    std::string err;
    fi_fail_at(FI_SNPRINTF, 1, EIO);                /* join_path for the source */
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "path too long"));
}

CASE(DirCopy, Errors, DestinationPathTooLong)
{
    std::string err;
    fi_fail_at(FI_SNPRINTF, 2, EIO);                /* join_path for the destination */
    EXPECT_EQ(-1, cpdir(opts(), nullptr, nullptr, &err));
    EXPECT_TRUE(contains(err, "path too long"));
}

/* ---------- worker / thread behaviour ---------- */
CASE(DirCopy, ThreadsAndWorkers, FailedFileIsCountedAndReported)
{
    stats_t st = {};
    std::string err;
    fi_fail_at(FI_OPEN, 1, EIO);                    /* first file cannot be opened */
    EXPECT_EQ(-1, cpdir(opts(), &st, nullptr, &err));
    EXPECT_EQ(1u, st.files_failed);
    EXPECT_EQ(2u, st.files_ok);
}

CASE(DirCopy, ThreadsAndWorkers, FailedFileWithThreads)
{
    make_many(20);
    options_t o = opts();
    o.threads = 4;
    stats_t st = {};
    fi_fail_at(FI_OPEN, 1, EIO);
    EXPECT_EQ(-1, cpdir(o, &st, nullptr, nullptr, p("many"), p("many_out")));
    EXPECT_EQ(1u, st.files_failed);
    EXPECT_EQ(19u, st.files_ok);
}

CASE(DirCopy, ThreadsAndWorkers, ThreadCreationAlwaysFailsFallsBackToCallingThread)
{
    options_t o = opts();
    o.threads = 4;
    stats_t st = {};
    fi_fail_from(FI_PTHREAD, 1, EAGAIN);
    EXPECT_EQ(0, cpdir(o, &st));
    EXPECT_EQ(3u, st.files_ok);
    expect_tree_copied();
}

CASE(DirCopy, ThreadsAndWorkers, ThreadCreationFailsPartway)
{
    make_many(12);
    options_t o = opts();
    o.threads = 4;
    stats_t st = {};
    fi_fail_at(FI_PTHREAD, 2, EAGAIN);              /* 2nd create fails, 3rd and 4th work */
    EXPECT_EQ(0, cpdir(o, &st, nullptr, nullptr, p("many"), p("many_out")));
    EXPECT_EQ(12u, st.files_ok);
}

/* ---- one gtest test per group ---- */
GROUP(DirCopy, Success)
GROUP(DirCopy, Permissions)
GROUP(DirCopy, Errors)
GROUP(DirCopy, ThreadsAndWorkers)
