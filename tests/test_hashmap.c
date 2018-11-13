#include <errno.h>              /* errno */
#include <string.h>             /* strerror */
#include <unistd.h>             /* unlink */

#include "../vendor/unity/unity.h"

#include "../src/hashmap.h"


hashmap_t map;


void setUp()
{
    /* Nothing to do */
}


void tearDown()
{
    hashmap_destroy(&map);
}


void test_hashmap_init()
{
    int rval;

    rval = hashmap_init(&map, 0);
    TEST_ASSERT_EQUAL_INT(-1, rval);
    hashmap_destroy(&map);

    rval = hashmap_init(&map, 10);
    TEST_ASSERT_EQUAL_INT(0, rval);
    /* destroyed in tearDown */
}


void test_hashmap_add_get_indices()
{
    int addidx, getidx;
    char *value;
    char *keys[] = {"a", "b", "c"};
    char *values[] = {"1", "2", "3"};

    hashmap_init(&map, 10);

    for (int i = 0; i < 3; i++) {
        addidx = hashmap_add(&map, keys[i], values[i]);
        getidx = hashmap_get(&map, keys[i], &value);

        TEST_ASSERT_GREATER_OR_EQUAL(0, addidx);
        TEST_ASSERT_GREATER_OR_EQUAL(0, getidx);
        TEST_ASSERT_EQUAL(addidx, getidx);
        TEST_ASSERT_EQUAL_STRING(values[i], value);

        free(value);
    }

}


void test_hashmap_add_get_force_hash_collision()
{
    int addidx, getidx;
    char *value;
    char *keys[] = {"a", "b", "c"};
    char *values[] = {"1", "2", "3"};

    hashmap_init(&map, 1);      /* bucket size 1 will force hash collision */

    for (int i = 0; i < 3; i++) {
        addidx = hashmap_add(&map, keys[i], values[i]);
        getidx = hashmap_get(&map, keys[i], &value);

        TEST_ASSERT_EQUAL(0, addidx);
        TEST_ASSERT_EQUAL(0, getidx);
        TEST_ASSERT_EQUAL(addidx, getidx);
        TEST_ASSERT_EQUAL_STRING(values[i], value);

        free(value);
    }

}


void test_hashmap_size()
{
    char *keys[] = {"a", "b", "c"};
    char *values[] = {"1", "2", "3"};

    hashmap_init(&map, 10);

    /* Adding a (key, value) should increase the map size by 1 */
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(i, map.size);
        hashmap_add(&map, keys[i], values[i]);
        TEST_ASSERT_EQUAL_INT(i + 1, map.size);
    }

    /* Changing the value of an existing key should not change size */
    hashmap_add(&map, keys[0], "new value");
    TEST_ASSERT_EQUAL_INT(3, map.size);

    /* Deleting a (key, value) should decrease the map size by 1 */
    for (int i = 2; i >= 0; i--) {
        TEST_ASSERT_EQUAL_INT(i + 1, map.size);
        hashmap_del(&map, keys[i]);
        TEST_ASSERT_EQUAL_INT(i, map.size);
    }

    /* Deleting a nonexistent key should not change size  */
    hashmap_del(&map, keys[0]);
    TEST_ASSERT_EQUAL_INT(0, map.size);
}


void test_hashmap_timeout_0_gc_noop()
{
    unsigned long start = time(NULL);

    hashmap_init(&map, 10);

    map.timeout = 0;

    hashmap_add(&map, "a", "1");

    while (((unsigned long) time(NULL)) == start)
        ;

    hashmap_gc(&map);
    TEST_ASSERT_EQUAL_INT(1, map.size);

    map.timeout = 1;

    while (((unsigned long) time(NULL)) == start + 1)
        ;

    hashmap_gc(&map);
    TEST_ASSERT_EQUAL_INT(0, map.size);
}


void test_hashmap_unlinker()
{
    FILE *file;
    const char fpath_fmt[] = "/tmp/test_hashmap_%lu";
    char *fpath[50];
    const char *fname;

    hashmap_init(&map, 10);

    /* "Touch" a temp file like test_hashmap_1542079712 */
    sprintf((char *) fpath, fpath_fmt, (unsigned long) time(NULL));
    fname = (const char *) fpath;
    file = fopen(fname, "w+");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, strerror(errno));

    /* Add fname to hashmap and then delete it, triggering the unlinker */
    map.unlinker = unlink;      /* set `unlink` to be called on value at del */
    hashmap_add(&map, "tempfile", fname);
    hashmap_del(&map, "tempfile");

    /* Verify file no longer exists */
    TEST_ASSERT_EQUAL_INT(-1, access(fname, F_OK));
}


int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_hashmap_init);
    RUN_TEST(test_hashmap_add_get_indices);
    RUN_TEST(test_hashmap_add_get_force_hash_collision);
    RUN_TEST(test_hashmap_size);
    /* Passes but takes about 2 seconds to run - normally disabled */
    //RUN_TEST(test_hashmap_timeout_0_gc_noop);
    RUN_TEST(test_hashmap_unlinker);

    return UNITY_END();
}
