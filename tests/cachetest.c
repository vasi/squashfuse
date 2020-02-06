#include "cache.h"
#include <stdio.h>

typedef struct {
    int x;
    int y;
} TestStruct;

static void TestStructDispose(void *t) {
    // nada.
}

#define EXPECT_EQ(exp1, exp2)						  \
	do { if ((exp1) != (exp2)) {					  \
		printf("Test failure: expected " #exp1 " to equal " #exp2 \
		       " at " __FILE__ ":%d\n", __LINE__);		  \
		++errors;						  \
	  }								  \
	} while (0)

#define EXPECT_NE(exp1, exp2)						   \
	do { if ((exp1) == (exp2)) {					   \
		printf("Test failure: expected " #exp1 " to !equal " #exp2 \
		       " at " __FILE__ ":%d\n", __LINE__);		   \
		++errors;						   \
          }								   \
	} while (0)


int test_cache_miss(void) {
    int errors = 0;
    sqfs_cache cache;
    TestStruct *entry;

    EXPECT_EQ(sqfs_cache_init(&cache, sizeof(TestStruct), 16,
                              TestStructDispose), SQFS_OK);
    entry = (TestStruct *)sqfs_cache_get(&cache, 1);
    EXPECT_EQ(sqfs_cache_entry_valid(&cache, entry), 0);
    sqfs_cache_put(&cache, entry);
    sqfs_cache_destroy(&cache);

    return errors == 0;
}

int test_mark_valid_and_lookup(void) {
    int errors = 0;
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
    return errors == 0;
}

int test_two_entries(void) {
    int errors = 0;
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

    return errors == 0;
}

int main(void) {
	return test_cache_miss() &&
		test_mark_valid_and_lookup() &&
		test_two_entries() ? 0 : 1;
}
