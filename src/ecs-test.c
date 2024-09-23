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
#include "koh_set.h"
// }}}

/*
 Нужны структуры разной длины для тестирования менеджера памяти компонентной
 системы.
 */
#define COMPONENT_NAME(n) Comp_## n

#define COMPONENT_DEFINE(n)                                        \
__attribute__((unused))                                            \
typedef struct {                                                   \
    uint32_t rng[n];                                               \
    int rng_num;                                                   \
    de_entity e;                                                   \
    UT_hash_handle hh;                                             \
} COMPONENT_NAME(n);                                               \
                                                                   \
de_cp_type cp_type_## n = {                                        \
    .cp_id = 0,                                                    \
    .name = #n,                                                    \
    .initial_cap = 0,                                              \
    .cp_sizeof = sizeof(COMPONENT_NAME(n)),                        \
    .str_repr = NULL,                                              \
};                                                                 \
                                                                   \
__attribute__((unused))                                            \
static COMPONENT_NAME(n) Comp_##n##_new(                           \
    xorshift32_state *rng, de_entity e                             \
) {                                                                \
    COMPONENT_NAME(n) ret = {                                      \
        .rng_num = n,                                              \
    };                                                             \
    assert(rng);                                                   \
    assert(n > 0);                                                 \
    ret.e = e;                                                     \
    for (int i = 0; i < n; i++) {                                  \
        ret.rng[i] = xorshift32_rand(rng);                         \
    }                                                              \
    return ret;                                                    \
}                                               

COMPONENT_DEFINE(1);
COMPONENT_DEFINE(5);
COMPONENT_DEFINE(17);
COMPONENT_DEFINE(73);

de_cp_type components[10] = {};
int components_num = 0;

static koh_SetAction iter_print(
    const void *key, int key_len, void *udata
) {
    printf("iter_print: e %u\n", *(de_entity*)key);
    return koh_SA_next;
}

static MunitResult test_emplace_1_insert_remove(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    xorshift32_state rng = xorshift32_init();
    de_ecs_register(r, cp_type_1);

    /*const int passes = 10000;*/
    const int passes = 10;

    koh_Set *set_c = set_new(NULL),
            *set_e = set_new(NULL);

    for (int i = 0; i < passes; ++i) {
        de_entity entt = de_create(r);
        printf("test_emplace_1_insert_remove: e %u\n", entt);
        Comp_1 comp = Comp_1_new(&rng, entt);

        Comp_1 *c = de_emplace(r, entt, cp_type_1);
        assert(c);

        *c = comp;
        set_add(set_c, c, sizeof(*c));
        set_add(set_e, &entt, sizeof(entt));
    }

    // Удалить часть сущностей
    int remove_count = passes / 3;
    // Выбрать случайные сущности из всех имеющихся
    de_entity remove_entts[remove_count];
    /*int remove_entts_num = 0;*/

    memset(remove_entts, 0, sizeof(remove_entts));

    printf("test_emplace_1_insert_remove: callback iterator\n");

    set_each(set_e, iter_print, NULL);

    printf("test_emplace_1_insert_remove: iterator\n");

    for (koh_SetView v = set_each_begin(set_e);
        set_each_valid(&v); set_each_next(&v)) {

        const de_entity *e = set_each_key(&v);
        assert(e);
        printf("test_emplace_1_insert_remove: e %u\n", *e);
    }

    /*while (remove_entts_num < remove_count) {*/
    /*}*/

    de_view_single view;
    for (view = de_view_single_create(r, cp_type_1);
         de_view_single_valid(&view);
         de_view_single_next(&view)) {

        de_entity e = de_view_single_entity(&view);
        munit_assert(set_exist(set_e, &e, sizeof(e)) == true);
        Comp_1 *c = de_view_single_get(&view);
        munit_assert(set_exist(set_c, c, sizeof(*c)) == true);
    }

    set_free(set_c);
    set_free(set_e);
    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitResult test_emplace_1_insert(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    xorshift32_state rng = xorshift32_init();

    de_ecs_register(r, cp_type_1);
    de_ecs_register(r, cp_type_5);
    de_ecs_register(r, cp_type_17);
    de_ecs_register(r, cp_type_73);

    const int passes = 10000;

    /*de_entity arr_e[passes] = {};*/
    /*Comp_1 arr_c[passes] = {};*/
    /*int arr_len = 0;*/
    koh_Set *set_c = set_new(NULL),
            *set_e = set_new(NULL);

    for (int i = 0; i < passes; ++i) {
        de_entity entt = de_create(r);
        Comp_1 comp = Comp_1_new(&rng, entt);
        /*arr_e[arr_len] = entt;*/
        /*arr_c[arr_len] = comp;*/

        Comp_1 *c = de_emplace(r, entt, cp_type_1);
        assert(c);

        *c = comp;
        set_add(set_c, c, sizeof(*c));
        set_add(set_e, &entt, sizeof(entt));
    }

    de_view_single view;
    for (view = de_view_single_create(r, cp_type_1);
         de_view_single_valid(&view);
         de_view_single_next(&view)) {

        de_entity e = de_view_single_entity(&view);
        munit_assert(set_exist(set_e, &e, sizeof(e)) == true);
        Comp_1 *c = de_view_single_get(&view);
        munit_assert(set_exist(set_c, c, sizeof(*c)) == true);
    }

    set_free(set_c);
    set_free(set_e);
    de_ecs_free(r);
    return MUNIT_OK;
}

/*
static MunitResult test_emplace(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    xorshift32_state rng = xorshift32_init();
    de_ecs_register(r, comp_1);

    const int passes = 10;

    Comp_1 *head = NULL;
    for (int i = 0; i < passes; ++i) {
        de_entity entt = de_create(r);
        Comp_1 comp = Comp_1_new(&rng, entt);

        Comp_1 *comp_hash = malloc(sizeof(*comp_hash));
        assert(comp_hash);
        *comp_hash = comp;
        printf("comp.e %u\n", comp.e);

        HASH_ADD_INT(head, e, comp_hash);

        *comp_hash = comp;
        // Comp_1 *comp_ecs = de_emplace(r, e, comp_1);
    }

    printf("------\n");

    for (Comp_1 *cur = head; cur && cur->hh.next; cur = cur->hh.next) {
        printf("%u\n", cur->e);
    }

    Comp_1 *comp, *tmp;
    HASH_ITER(hh, head, comp, tmp) {
        free(comp);
    }

    de_ecs_free(r);
    return MUNIT_OK;
}
*/

static MunitResult test_new_free(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {

    {
        (char*) "/new_free",
        test_new_free,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },

    {
        (char*) "/emplace_1_insert",
        test_emplace_1_insert,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
    },

    {
        (char*) "/emplace_1_insert_remove",
        test_emplace_1_insert_remove,
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

    components[components_num++] = cp_type_1;
    components[components_num++] = cp_type_5;
    components[components_num++] = cp_type_17;
    components[components_num++] = cp_type_73;

    return munit_suite_main(&test_suite, (void*) "µnit", argc, argv);
}
