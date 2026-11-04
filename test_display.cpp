#include "common.hpp"
#include <pwd.h>

class Display : public MycpTest {
protected:
    static struct stat fake(mode_t mode, uid_t uid = 0, off_t size = 0)
    {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_mode = mode;
        st.st_uid = uid;
        st.st_size = size;
        st.st_ino = 777;
        st.st_nlink = 3;
        st.st_mtime = 86400 * 365;
        return st;
    }
};

CASE(Display, FileInfo, RegularFileInfo)
{
    struct stat st = fake(S_IFREG | 0644, 0, 1536);
    Capture c;
    print_file_info("Source", "/x/file", &st);
    const std::string &o = c.out();
    EXPECT_TRUE(contains(o, "---- Source file info ----"));
    EXPECT_TRUE(contains(o, "Path        : /x/file"));
    EXPECT_TRUE(contains(o, "Size        : 1536 bytes (1.50 KB)"));
    EXPECT_TRUE(contains(o, "Permissions : -rw-r--r-- (0644)"));
    EXPECT_TRUE(contains(o, "Owner       : root (uid 0)"));
    EXPECT_TRUE(contains(o, "Inode       : 777"));
    EXPECT_TRUE(contains(o, "Hard links  : 3"));
    EXPECT_TRUE(contains(o, "Modified    : 19"));
}

CASE(Display, FileInfo, DirectoryTypeLetter)
{
    struct stat st = fake(S_IFDIR | 0755);
    Capture c;
    print_file_info("Dir", "/d", &st);
    EXPECT_TRUE(contains(c.out(), "drwxr-xr-x (0755)"));
}

CASE(Display, FileInfo, OtherTypeUsesQuestionMark)
{
    struct stat st = fake(S_IFIFO | 0600);
    Capture c;
    print_file_info("Pipe", "/p", &st);
    EXPECT_TRUE(contains(c.out(), "?rw------- (0600)"));
}

CASE(Display, FileInfo, AllPermissionBitsSet)
{
    struct stat st = fake(S_IFREG | 0777);
    Capture c;
    print_file_info("A", "/a", &st);
    EXPECT_TRUE(contains(c.out(), "-rwxrwxrwx (0777)"));
}

CASE(Display, FileInfo, NoPermissionBitsSet)
{
    struct stat st = fake(S_IFREG);
    Capture c;
    print_file_info("A", "/a", &st);
    EXPECT_TRUE(contains(c.out(), "---------- (0000)"));
}

CASE(Display, FileInfo, SpecialBitsAreShownInOctal)
{
    struct stat st = fake(S_IFREG | 04755);
    Capture c;
    print_file_info("A", "/a", &st);
    EXPECT_TRUE(contains(c.out(), "(4755)"));
}

CASE(Display, FileInfo, UnknownOwnerPrintsQuestionMark)
{
    struct stat st = fake(S_IFREG | 0644, 4242);
    fi_fail_at(FI_GETPWUID, 1, 1);
    Capture c;
    print_file_info("A", "/a", &st);
    EXPECT_TRUE(contains(c.out(), "Owner       : ? (uid 4242)"));
}

/* ---- progress bar ---- */
CASE(Display, ProgressBar, ProgressIgnoresZeroTotal)
{
    Capture c;
    progress_start();
    progress_update(0, 0);
    progress_finish();
    EXPECT_EQ("", c.out());
}

CASE(Display, ProgressBar, ProgressDrawsBar)
{
    Capture c;
    progress_start();
    progress_update(0, 100);
    progress_update(50, 100);
    progress_update(100, 100);
    progress_finish();
    const std::string &o = c.out();
    EXPECT_TRUE(contains(o, "[..............................]   0%"));
    EXPECT_TRUE(contains(o, "[###############...............]  50%"));
    EXPECT_TRUE(contains(o, "[##############################] 100%"));
    EXPECT_EQ('\n', o.back());
}

CASE(Display, ProgressBar, ProgressSamePercentPrintsOnce)
{
    Capture c;
    progress_start();
    progress_update(10, 1000);        /* 1%  */
    progress_update(11, 1000);        /* 1% again -> suppressed */
    progress_update(19, 1000);        /* 1% again -> suppressed */
    progress_finish();
    const std::string &o = c.out();
    size_t first = o.find("  1%");
    ASSERT_NE(std::string::npos, first);
    EXPECT_EQ(std::string::npos, o.find("  1%", first + 1));
}

CASE(Display, ProgressBar, ProgressClampsOver100)
{
    Capture c;
    progress_start();
    progress_update(500, 100);
    progress_finish();
    EXPECT_TRUE(contains(c.out(), "100%"));
}

CASE(Display, ProgressBar, FinishWithoutUpdatePrintsNothing)
{
    Capture c;
    progress_start();
    progress_finish();
    progress_finish();
    EXPECT_EQ("", c.out());
}

CASE(Display, ProgressBar, StartResetsState)
{
    Capture c;
    progress_start();
    progress_update(50, 100);
    progress_start();                 /* forget the last percentage */
    progress_update(50, 100);         /* must be drawn again */
    progress_finish();
    const std::string &o = c.out();
    size_t first = o.find(" 50%");
    ASSERT_NE(std::string::npos, first);
    EXPECT_NE(std::string::npos, o.find(" 50%", first + 1));
}

/* ---- one gtest test per group ---- */
GROUP(Display, FileInfo)
GROUP(Display, ProgressBar)
