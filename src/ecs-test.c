// vim: set colorcolumn=85
// vim: fdm=marker

// {{{ include
#include "koh_destral_ecs.h"
#include "koh_destral_ecs_internal.h"
#include "munit.h"
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uthash.h"
#include "koh_rand.h"
// }}}

/*
 Нужны структуры разной длины для тестирования менеджера памяти компонентной
 системы.
 */
#define COMPONENT_DEFINE(n)                     \
typedef struct {                                \
    uint32_t rng[n];                            \
    int rng_num;                                \
} Comp_##n;                                     \
                                                \
Comp_##n comp##n_new(xorshift32_state *rng) {   \
    Comp_##n ret = {                            \
        .rng_num = n,                           \
    };                                          \
    assert(rng);                                \
    assert(n > 0);                              \
    for (int i = 0; i < n; i++) {               \
        ret.rng[i] = xorshift32_rand(rng);      \
    }                                           \
    return ret;                                 \
}                                               

/*
typedef struct {
    xorshift32_state rng[1];
    int rng_num;
} Comp2;

typedef struct {
    xorshift32_state rng[1];
    int rng_num;
} Comp3;
*/

COMPONENT_DEFINE(1);
COMPONENT_DEFINE(2);
COMPONENT_DEFINE(3);

static MunitResult test_sparse_ecs(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();




    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {

    {
        (char*) "/sparse_ecs",
        test_sparse_ecs,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },

    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite test_suite = {
  (char*) "de_ecs", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char **argv) {
    koh_hashers_init();
    return munit_suite_main(&test_suite, (void*) "µnit", argc, argv);
}
