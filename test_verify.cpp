#include "common.hpp"
#include <cerrno>

class Verify : public MycpTest {};

CASE(Verify, Comparison, IdenticalFiles)
{
    put(p("a"), pattern(10000));
    put(p("b"), pattern(10000));
    EXPECT_EQ(0, verify_files(p("a").c_str(), p("b").c_str(), 4096));
}

CASE(Verify, Comparison, IdenticalWithTinyBuffer)
{
    put(p("a"), pattern(100));
    put(p("b"), pattern(100));
    EXPECT_EQ(0, verify_files(p("a").c_str(), p("b").c_str(), 1));
}

CASE(Verify, Comparison, BothEmpty)
{
    put(p("a"), "");
    put(p("b"), "");
    EXPECT_EQ(0, verify_files(p("a").c_str(), p("b").c_str(), 4096));
}

CASE(Verify, Comparison, SameSizeDifferentContent)
{
    std::string d = pattern(9000);
    put(p("a"), d);
    d[8500] ^= 0x55;
    put(p("b"), d);
    EXPECT_EQ(1, verify_files(p("a").c_str(), p("b").c_str(), 4096));
}

CASE(Verify, Comparison, DifferentSizes)
{
    put(p("a"), pattern(5000));
    put(p("b"), pattern(4096));
    EXPECT_EQ(1, verify_files(p("a").c_str(), p("b").c_str(), 4096));
    EXPECT_EQ(1, verify_files(p("b").c_str(), p("a").c_str(), 4096));
}

CASE(Verify, Comparison, OneFileEmpty)
{
    put(p("a"), "x");
    put(p("b"), "");
    EXPECT_EQ(1, verify_files(p("a").c_str(), p("b").c_str(), 4096));
}

CASE(Verify, Errors, MissingFirstFile)
{
    put(p("b"), "x");
    Capture c;
    EXPECT_EQ(-1, verify_files(p("nope").c_str(), p("b").c_str(), 4096));
    EXPECT_TRUE(contains(c.err(), "verify: cannot open"));
}

CASE(Verify, Errors, MissingSecondFile)
{
    put(p("a"), "x");
    Capture c;
    EXPECT_EQ(-1, verify_files(p("a").c_str(), p("nope").c_str(), 4096));
}

CASE(Verify, Errors, BothMissing)
{
    Capture c;
    EXPECT_EQ(-1, verify_files(p("x").c_str(), p("y").c_str(), 4096));
}

CASE(Verify, Errors, FirstAllocationFails)
{
    put(p("a"), "x"); put(p("b"), "x");
    fi_fail_at(FI_MALLOC, 1, ENOMEM);
    Capture c;
    EXPECT_EQ(-1, verify_files(p("a").c_str(), p("b").c_str(), 4096));
}

CASE(Verify, Errors, SecondAllocationFails)
{
    put(p("a"), "x"); put(p("b"), "x");
    fi_fail_at(FI_MALLOC, 2, ENOMEM);
    Capture c;
    EXPECT_EQ(-1, verify_files(p("a").c_str(), p("b").c_str(), 4096));
}

CASE(Verify, Errors, ReadErrorOnFirstFile)
{
    put(p("a"), "x"); put(p("b"), "x");
    fi_fail_at(FI_READ, 1, EIO);
    Capture c;
    EXPECT_EQ(-1, verify_files(p("a").c_str(), p("b").c_str(), 4096));
    EXPECT_TRUE(contains(c.err(), "verify: read error"));
}

CASE(Verify, Errors, ReadErrorOnSecondFile)
{
    put(p("a"), "x"); put(p("b"), "x");
    /* read_full(a) needs two read() calls (data + EOF), so the first read of
       file b is call number 3. */
    fi_fail_at(FI_READ, 3, EIO);
    Capture c;
    EXPECT_EQ(-1, verify_files(p("a").c_str(), p("b").c_str(), 4096));
    EXPECT_TRUE(contains(c.err(), "verify: read error"));
}

CASE(Verify, Errors, DirectoryCannotBeRead)
{
    put(p("b"), "x");
    Capture c;
    EXPECT_EQ(-1, verify_files(dir.c_str(), p("b").c_str(), 4096));
}

/* ---- one gtest test per group ---- */
GROUP(Verify, Comparison)
GROUP(Verify, Errors)
