#include <gtest/gtest.h>

extern "C" {
#include "cache.h"
}

struct TestStruct {
    int x;
    int y;
};

static void TestStructDispose(void *) {
    // nada.
}

TEST(cachetest, CacheMiss) {
    sqfs_cache cache;
    TestStruct *entry;

    EXPECT_EQ(sqfs_cache_init(&cache, sizeof(TestStruct), 16,
                              TestStructDispose), SQFS_OK);
    entry = (TestStruct *)sqfs_cache_get(&cache, 1);
    EXPECT_EQ(sqfs_cache_entry_valid(&cache, entry), 0);
    sqfs_cache_put(&cache, entry);
    sqfs_cache_destroy(&cache);
}

TEST(cachetest, MarkValidAndLookup) {
    sqfs_cache cache;
    TestStruct *entry;

    EXPECT_EQ(sqfs_cache_init(&cache, sizeof(TestStruct), 16,
                              TestStructDispose), SQFS_OK);
    entry = (TestStruct *)sqfs_cache_get(&cache, 1);
    entry->x = 666;
    entry->y = 777;
    sqfs_cache_entry_mark_valid(&cache, entry);
    sqfs_cache_put(&cache, entry);
    EXPECT_NE(sqfs_cache_entry_valid(&cache, entry), 0);
    entry = (TestStruct *)sqfs_cache_get(&cache, 1);
    EXPECT_NE(sqfs_cache_entry_valid(&cache, entry), 0);
    EXPECT_EQ(entry->x, 666);
    EXPECT_EQ(entry->y, 777);
    sqfs_cache_put(&cache, entry);

    sqfs_cache_destroy(&cache);
}

TEST(cachetest, TwoEntries) {
    sqfs_cache cache;
    TestStruct *entry1, *entry2;

    EXPECT_EQ(sqfs_cache_init(&cache, sizeof(TestStruct), 16,
                              TestStructDispose), SQFS_OK);

    entry1 = (TestStruct *)sqfs_cache_get(&cache, 1);
    entry1->x = 1;
    entry1->y = 2;
    sqfs_cache_entry_mark_valid(&cache, entry1);
    sqfs_cache_put(&cache, entry1);

    entry2 = (TestStruct *)sqfs_cache_get(&cache, 666);
    entry2->x = 3;
    entry2->y = 4;
    sqfs_cache_entry_mark_valid(&cache, entry2);
    sqfs_cache_put(&cache, entry2);

    entry1 = (TestStruct *)sqfs_cache_get(&cache, 1);
    sqfs_cache_put(&cache, entry1);
    entry2 = (TestStruct *)sqfs_cache_get(&cache, 666);
    sqfs_cache_put(&cache, entry2);
    EXPECT_NE(sqfs_cache_entry_valid(&cache, entry1), 0);
    EXPECT_NE(sqfs_cache_entry_valid(&cache, entry2), 0);
    EXPECT_EQ(entry1->x, 1);
    EXPECT_EQ(entry1->y, 2);
    EXPECT_EQ(entry2->x, 3);
    EXPECT_EQ(entry2->y, 4);

    sqfs_cache_destroy(&cache);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
