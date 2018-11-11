#include <stdlib.h>

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
    }

}



/*
 * void test_hashmap_timeout_0_gc_noop()
 * {
 *     hashmap_init(&map, 10);
 *
 *
 * }
 */


int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_hashmap_init);
    RUN_TEST(test_hashmap_add_get_indices);
    RUN_TEST(test_hashmap_add_get_force_hash_collision);

    return UNITY_END();
}
