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
#include "koh_logger.h"
#include "koh_table.h"
//#include "koh_strset.h"

#include "koh_set.h"
// }}}

static const MunitSuite test_suite;

/*
 Нужны структуры разной длины для тестирования менеджера памяти компонентной
 системы.
 */
#define COMPONENT_NAME(n) Comp_## n

// TODO: Автоматизировать поиск через UT_hash_handle

typedef void (*ComponentCreator)(
    xorshift32_state *rnd, de_entity e, void *retp
);

// {{{ Определение компонента с конструктором экземпляра объекта
#define COMPONENT_DEFINE(n)                                        \
__attribute__((unused))                                            \
typedef struct {                                                   \
    int32_t rng[n];                                                \
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
static void Comp_##n##_new(                                        \
    xorshift32_state *rng, de_entity e, COMPONENT_NAME(n) *retp    \
) {                                                                \
    COMPONENT_NAME(n) ret = {                                      \
        .rng_num = n,                                              \
    };                                                             \
    assert(retp);                                                  \
    assert(rng);                                                   \
    assert(n > 0);                                                 \
    ret.e = e;                                                     \
    for (int i = 0; i < n; i++) {                                  \
        ret.rng[i] = xorshift32_rand(rng);                         \
    }                                                              \
    *retp = ret;                                                   \
}                                               
// }}}

// В скобках - размер массива со случайными данными, которые заполняются
// при конструировании объекта соответсующей функцией - конструктором.
// Массив чисел заданной длинны позволяет проверить наличие затирания памяти
// со стороны de_ecs в компоненте.
COMPONENT_DEFINE(1);
COMPONENT_DEFINE(5);
COMPONENT_DEFINE(17);
COMPONENT_DEFINE(73);

de_cp_type components[10] = {};
int components_num = 0;

struct Cell {
    int             value;
    int             from_x, from_y, to_x, to_y;
    bool            moving;
};

struct Triple {
    float dx, dy, dz;
};

struct Node {
    char u;
};

static bool verbose_print = false;

static const de_cp_type cp_triple = {
    .cp_id = 0,
    .cp_sizeof = sizeof(struct Triple),
    .name = "triple",
    .initial_cap = 2000,
};

static const de_cp_type cp_cell = {
    .cp_id = 1,
    .cp_sizeof = sizeof(struct Cell),
    .name = "cell",
    .initial_cap = 20000,
};

static const de_cp_type cp_node = {
    .cp_id = 2,
    .cp_sizeof = sizeof(struct Node),
    .name = "node",
    .initial_cap = 20,
};

static void map_on_remove(
    const void *key, int key_len, void *value, int value_len, void *userdata
) { 
    printf("map_on_remove:\n");
}

// TODO: использовать эту функцию для тестирования сложносоставных сущностей
static struct Triple *create_triple(
    de_ecs *r, de_entity en, const struct Triple tr
) {

    assert(r);
    assert(de_valid(r, en));
    struct Triple *triple = de_emplace(r, en, cp_triple);
    munit_assert_ptr_not_null(triple);
    *triple = tr;

    if (verbose_print)
        trace(
            "create_tripe: en %u at (%f, %f, %f)\n", 
            en, triple->dx, triple->dy, triple->dz
        );

    return triple;
}

static struct Cell *create_cell(de_ecs *r, int x, int y, de_entity *e) {

    //if (get_cell_count(mv) >= FIELD_SIZE * FIELD_SIZE)
        //return NULL;

    de_entity en = de_create(r);
    munit_assert(en != de_null);
    struct Cell *cell = de_emplace(r, en, cp_cell);
    munit_assert_ptr_not_null(cell);
    cell->from_x = x;
    cell->from_y = y;
    cell->to_x = x;
    cell->to_y = y;
    cell->value = -1;

    if (e) *e = en;

    if (verbose_print)
        trace(
            "create_cell: en %u at (%d, %d)\n",
            en, cell->from_x, cell->from_y
        );
    return cell;
}

static bool iter_set_add_mono(de_ecs* r, de_entity en, void* udata) {
    HTable *entts = udata;

    struct Cell *cell = de_try_get(r, en, cp_cell);
    munit_assert_ptr_not_null(cell);

    char repr_cell[256] = {};
    if (verbose_print)
        sprintf(repr_cell, "en %u, cell %d %d %s %d %d %d", 
            en,
            cell->from_x, cell->from_y,
            cell->moving ? "t" : "f",
            cell->to_x, cell->to_y,
            cell->value
        );

    /*strset_add(entts, repr_cell);*/
    htable_add_s(entts, repr_cell, NULL, 0);
    return false;
}

static bool iter_set_add_multi(de_ecs* r, de_entity en, void* udata) {
    /*StrSet *entts = udata;*/
    HTable *entts = udata;

    struct Cell *cell = de_try_get(r, en, cp_cell);
    munit_assert_ptr_not_null(cell);

    struct Triple *triple = de_try_get(r, en, cp_triple);
    munit_assert_ptr_not_null(triple);

    char repr_cell[256] = {};
    if (verbose_print)
        sprintf(repr_cell, "en %u, cell %d %d %s %d %d %d", 
            en,
            cell->from_x, cell->from_y,
            cell->moving ? "t" : "f",
            cell->to_x, cell->to_y,
            cell->value
        );

    char repr_triple[256] = {};
    if (verbose_print)
        sprintf(repr_triple, "en %u, %f %f %f", 
            en,
            triple->dx,
            triple->dy,
            triple->dz
        );

    char repr[strlen(repr_cell) + strlen(repr_triple) + 2];
    memset(repr, 0, sizeof(repr));

    strcat(strcat(repr, repr_cell), repr_triple);

    /*strset_add(entts, repr);*/
    htable_add_s(entts, repr, NULL, 0);
    return false;
}

/*
static koh_SetAction iter_set_print(
    const void *key, int key_len, void *udata
) {
    munit_assert(key_len == sizeof(de_entity));
    const de_entity *en = key;
    //printf("(%f, %f)\n", vec->x, vec->y);
    printf("en %u\n", *en);
    return koh_SA_next;
}
*/

static MunitResult test_try_get_none_existing_component(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    //for (int i = 0; i < 5000; ++i) {
    for (int i = 0; i < 50; ++i) {
        de_entity en = de_create(r);

        struct Cell *cell;
        struct Triple *triple;

        cell = de_emplace(r, en, cp_cell);
        cell->moving = true;

        cell = NULL;
        cell = de_try_get(r, en, cp_cell);
        assert(cell);

        ///////////// !!!!!
        triple = NULL;
        triple = de_try_get(r, en, cp_triple);
        assert(!triple);
        ///////////// !!!!!

        cell = NULL;
        cell = de_try_get(r, en, cp_cell);
        assert(cell);
    }

    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitResult test_ecs_clone_multi(
    const MunitParameter params[], void* data
) {

    de_ecs *r = de_ecs_new();

    for (int x = 0; x < 50; x++) {
        for (int y = 0; y < 50; y++) {
            de_entity en = de_null;
            struct Cell *cell = create_cell(r, x, y, &en);
            munit_assert(en != de_null);
            create_triple(r, en, (struct Triple) {
                .dx = x,
                .dy = y,
                .dz = x * y,
            });
            munit_assert_ptr_not_null(cell);
        }
    }
    de_ecs  *cloned = de_ecs_clone(r);

    /*StrSet *entts1 = strset_new(NULL);*/
    /*StrSet *entts2 = strset_new(NULL);*/
    HTable *entts1 = htable_new(NULL);
    HTable *entts2 = htable_new(NULL);

    de_each(r, iter_set_add_multi, entts1);
    de_each(cloned, iter_set_add_multi, entts2);

    /*
    printf("\n"); printf("\n"); printf("\n");
    set_each(entts1, iter_set_print, NULL);

    printf("\n"); printf("\n"); printf("\n");
    set_each(entts2, iter_set_print, NULL);
    */

    /*munit_assert(strset_compare(entts1, entts2));*/
    munit_assert(htable_compare_keys(entts1, entts2));

    htable_free(entts1);
    htable_free(entts2);
    de_ecs_free(r);
    de_ecs_free(cloned);

    return MUNIT_OK;
}

// TODO: Добавить проверку компонент сущностей
static MunitResult test_ecs_clone_mono(
    const MunitParameter params[], void* data
) {

    de_ecs *r = de_ecs_new();

    for (int x = 0; x < 50; x++) {
        for (int y = 0; y < 50; y++) {
            struct Cell *cell = create_cell(r, x, y, NULL);
            munit_assert_ptr_not_null(cell);
        }
    }
    de_ecs  *cloned = de_ecs_clone(r);
    HTable *entts1 = htable_new(NULL);
    HTable *entts2 = htable_new(NULL);

    de_each(r, iter_set_add_mono, entts1);
    de_each(cloned, iter_set_add_mono, entts2);

    /*
    printf("\n"); printf("\n"); printf("\n");
    strset_each(entts1, iter_strset_print, NULL);

    printf("\n"); printf("\n"); printf("\n");
    strset_each(entts2, iter_strset_print, NULL);
    */

    munit_assert(htable_compare_keys(entts1, entts2));

    htable_free(entts1);
    htable_free(entts2);
    de_ecs_free(r);
    de_ecs_free(cloned);

    return MUNIT_OK;
}

/*
static MunitResult test_emplace_destroy_with_hash(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    table = htable_new(&(struct HTableSetup) {
        .cap = 30000,
        .on_remove = NULL,
    });

    for (int k = 0; k < 10; k++) {

        for (int x = 0; x < 100; x++) {
            for (int y = 0; y < 100; y++) {
                struct Cell *cell = create_cell(r, x, y);
                munit_assert_ptr_not_null(cell);
            }
        }

        for (int j = 0; j < 10; j++) {

            for (int i = 0; i < 10; i++) {

                for (de_view v = de_create_view(r, 1, (de_cp_type[1]) { cp_cell }); 
                        de_view_valid(&v); de_view_next(&v)) {

                    munit_assert(de_valid(r, de_view_entity(&v)));
                    struct Cell *c = de_view_get_safe(&v, cp_cell);
                    munit_assert_ptr_not_null(c);

                    c->moving = false;
                    c->from_x = rand() % 100 + 10;
                    c->from_y = rand() % 100 + 10;
                }

            }

            for (de_view v = de_create_view(r, 1, (de_cp_type[1]) { cp_cell }); 
                    de_view_valid(&v); de_view_next(&v)) {


                munit_assert(de_valid(r, de_view_entity(&v)));
                struct Cell *c = de_view_get_safe(&v, cp_cell);
                munit_assert_ptr_not_null(c);

                if (c->from_x == 10 || c->from_y == 10) {
                    printf("removing entity\n");
                    de_destroy(r, de_view_entity(&v));
                }
            }

        }
    }

    for (de_view v = de_create_view(r, 1, (de_cp_type[1]) { cp_cell }); 
        de_view_valid(&v); de_view_next(&v)) {

        munit_assert(de_valid(r, de_view_entity(&v)));
        struct Cell *c = de_view_get_safe(&v, cp_cell);
        munit_assert_ptr_not_null(c);

        munit_assert_int(c->from_x, >=, 10);
        munit_assert_int(c->from_x, <=, 10 + 100);
        munit_assert_int(c->from_y, >=, 10);
        munit_assert_int(c->from_y, <=, 10 + 100);
        munit_assert(c->moving == false);
    }

    de_ecs_free(r);
    htable_free(table);
    table = NULL;
    return MUNIT_OK;
}
*/

// Как сохранить данные сущностей?
typedef struct EntityDesc {
    // Какие типы присутствуют в сущности
    de_cp_type  types[3];

    // Память под компоненты
    // XXX: Какую память использовать? 
    // Делать копию или достаточно на указатель возвращенный de_emplace?
    void        *components[3];

    // Размеры выделенной памяти под компоненты
    size_t      components_sizes[3];

    // Количество прикрепленных к сущности компонент
    size_t      components_num;
} EntityDesc;

/*
Эмуляция de_ecs через HTable
Максимально используется 3 компоненты на сущность.
 */
typedef struct TestDestroyCtx {
    de_ecs              *r;

    // Какие компоненты цеплять у создаваемым сущностям
    de_cp_type          types[3];
    int                 types_num;

    // Указатели на функции для заполнения компонент
    ComponentCreator    creators[3];

    HTable              *map_entt2Desc;
} TestDestroyCtx;

// Удаляет одну случайную сущность из системы.
static struct TestDestroyCtx facade_entt_destroy(
    struct TestDestroyCtx ctx,
    bool partial_remove // удалить случайную часть компонент сущности
) {
    assert(ctx.r);

    if (verbose_print)
        printf("facade_entt_destroy:\n");

    int n = rand() % htable_count(ctx.map_entt2Desc);
    printf("facade_entt_destroy: n %d\n", n);

    int j = n;
    for (HTableIterator i = htable_iter_new(ctx.map_entt2Desc);
         htable_iter_valid(&i); htable_iter_next(&i)) {
        j--;
        if (!j) {
            // удалить этот элемент


            EntityDesc *ed = htable_iter_value(&i, NULL);
            munit_assert(ed != NULL);

            for (int t = 0; ed->components_num; t++) {
                if (ed->components[t]) {
                    // Кто владеет памятью?
                    free(ed->components[t]);
                    ed->components[t] = NULL;
                }
            }

            int key_len = 0;
            void *key = htable_iter_key(&i, &key_len);
            htable_remove(ctx.map_entt2Desc, key, key_len);
            break;
        }
    }

    return ctx;
}

// Создает одну сущность и цепляет к ней по данных из ctx какое-то количество
// компонент
// Может вернуть сущность если предоставлен указатель.
static TestDestroyCtx facade_ennt_create(TestDestroyCtx ctx, de_entity *ret) {
    assert(ctx.r);
    assert(ctx.map_entt2Desc);
    assert(ctx.types_num >= 0); // XXX: Дать строгое неравенство?
    assert(ctx.types_num < 3);

    if (verbose_print)
        printf("facade_ennt_create:\n");

    de_entity e = de_create(ctx.r);
    munit_assert(e != de_null);
    munit_assert(de_valid(ctx.r, e));

    EntityDesc ed = {};

    ed.components_num = ctx.types_num;

    for (int k = 0; k < ctx.types_num; k++) {
        void *comp_data = de_emplace(ctx.r, e, ctx.types[k]);
        // TODO: здесь вызвать функцию заполнения comp_data данными
        ed.components_sizes[k] = ctx.types[k].cp_sizeof;
        ed.components[k] = comp_data;
    }

    assert(ctx.map_entt2Desc);
    htable_add(ctx.map_entt2Desc, &e, sizeof(e), &ed, sizeof(ed));

    if (ret)
        *ret = e;

    return ctx;
}

/*
static HTableAction  set_print_each(
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    assert(key);
    printf("    %s\n", estate2str(key));
    return HTABLE_ACTION_NEXT;
}
*/

/*
static void estate_set_print(HTable *set) {
    if (verbose_print) {
        printf("estate {\n");
        //set_each(set, set_print_each, NULL);
        htable_each(set, set_print_each, NULL);
        printf("} (size = %ld)\n", htable_count(set));
    }
}
*/

/*
static bool iter_ecs_each(de_ecs *r, de_entity e, void *udata) {
    struct TestDestroyCtx *ctx = udata;

    assert(r);
    assert(udata);
    assert(e != de_null);
    assert(ctx->set);

    struct EntityState estate = { 
        .e = e, 
        .found = false,
    };

    // сборка структуры estate через запрос к ecs
    for (int i = 0; i < ctx->components_num; i++) {
        if (de_has(r, e, ctx->components[i])) {
            estate.components_set[i] = true;
            int *comp_value = de_try_get(r, e, ctx->components[i]);
            munit_assert_ptr_not_null(comp_value);
            estate.components_values[i] = *comp_value;
        }
    }

    if (verbose_print)
        printf("iter_ecs_each: search estate %s\n", estate2str(&estate));

    bool exists = htable_exist(ctx->set, &estate, sizeof(estate));

    if (verbose_print)
        printf("estate {\n");

    for (HTableIterator v = htable_iter_new(ctx->set);
            htable_iter_valid(&v); htable_iter_next(&v)) {

        const struct EntityState *key = htable_iter_key(&v, NULL);
        
        if (!key) {
            fprintf(stderr, "set_each_key return NULL\n");
            abort();
        }

        if (!memcmp(key, &estate, sizeof(estate))) {
            exists = true;
        }
        if (verbose_print)
            printf("    %s\n", estate2str(key));
    }
    if (verbose_print)
        printf("} (size = %ld)\n", htable_count(ctx->set));
    
    if (!exists) {
        if (verbose_print)
            printf("iter_ecs_each: not found\n");
        estate_set_print(ctx->set);
        munit_assert(exists);
    } else {
        if (verbose_print)
            printf("iter_ecs_each: EXISTS %s\n", estate2str(&estate));
    }

    return false;
}
*/

bool iter_ecs_each(de_ecs* r, de_entity e, void* ud) {
    TestDestroyCtx *ctx = ud;

    munit_assert(e != de_null);

    if (htable_count(ctx->map_entt2Desc) == 0)
        return false;

    EntityDesc *ed = htable_get(ctx->map_entt2Desc, &e, sizeof(e), NULL);

    if (!ed) {
        /*printf("iter_ecs_each:\n");*/
        /*return false;*/
    }

    munit_assert(ed != NULL);
    for (int i = 0; i < ctx->types_num; i++) {
        if (de_has(ctx->r, e, ctx->types[i])) {

        }
    }

    return false;
}

struct TestDestroyCtx facade_compare_with_ecs(struct TestDestroyCtx ctx) {
    assert(ctx.r);
    // TODO: Пройтись по всем сущностям. 
    // Найти через de_has() какие сущности есть согласно EntityDesc
    // Сравнить память через memcmp()
    /*de_each(ctx.r, iter_ecs_each, &ctx);*/

    // de_each(ctx.r, iter_ecs_each, &ctx);

    de_each_iter i = de_each_begin(ctx.r);
    for (; de_each_valid(&i); de_each_next(&i)) {
        de_entity e = de_each_entity(&i);
        EntityDesc *ed = htable_get(ctx.map_entt2Desc, &e, sizeof(e), NULL);
        munit_assert(ed != NULL);
    }

    return ctx;
}

static bool iter_ecs_counter(de_ecs *r, de_entity e, void *udata) {
    int *counter = udata;
    (*counter)++;
    return false;
}

/*
struct TestDestroyOneRandomCtx {
    de_entity   *entts;
    int         entts_len;
    de_cp_type  comp_type;
};
*/

// Все сущности имеют один компонент
/*
static bool iter_ecs_check_entt(de_ecs *r, de_entity e, void *udata) {
    struct TestDestroyOneRandomCtx *ctx = udata;
    for (int i = 0; i < ctx->entts_len; i++) {
        if (ctx->entts[i] == e) {
            munit_assert_true(de_has(r, e, ctx->comp_type));
        }
    }
    munit_assert(false);
    return false;
}
*/

// Сложный тест, непонятно, что он делает
/*
static MunitResult test_destroy_one_random(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    int entts_len = 4000;
    de_entity entts[entts_len];

    const static de_cp_type comp = {
        .cp_id = 0,
        .cp_sizeof = sizeof(int),
        .initial_cap = 1000,
        .name = "main component",
    };

    struct TestDestroyOneRandomCtx ctx = {
        .entts_len = entts_len,
        .entts = entts,
        .comp_type = comp,
    };

    for (int i = 0; i < entts_len; i++)
        entts[i] = de_null;
    //memset(entts, 0, sizeof(entts[0]) * entts_len);

    const int cycles = 1000;

    int comp_value_index = 0;
    for (int j = 0; j < cycles; j++) {

        // Добавляет в массив случайное количество сущносте с прикрепленной
        // компонентой
        int new_num = random() % 10;
        for (int i = 0; i < new_num; i++) {

            bool created = false;
            for (int k = 0; k < entts_len; ++k) {
                if (created)
                    break;

                if (entts[k] == de_null) {
                    //printf("creation cycle, k %d\n", k);
                    entts[k] = de_create(r);
                    munit_assert_uint32(entts[k], !=, de_null);
                    int *comp_value = de_emplace(r, entts[k], comp);
                    munit_assert_ptr_not_null(comp_value);
                    *comp_value = comp_value_index++;
                    created = true;
                }
            }
            munit_assert_true(created);

        }

        // Удаление случайно количество сущностей
        int destroy_num = random() % 5;
        for (int i = 0; i < destroy_num; ++i) {
            for (int k = 0; k < entts_len; ++k) {
                if (entts[k] != de_null) {
                    de_destroy(r, entts[k]);
                    entts[k] = de_null;
                }
            }
        }

    }

    // Все сущности имеют один компонент, проверка
    de_each(r, iter_ecs_check_entt, &ctx);

    // Удаление всех сущностей
    for (int k = 0; k < entts_len; ++k) {
        if (entts[k] != de_null)
            de_destroy(r, entts[k]);
    }

    // Проверка - сущностей не должно остаться
    int counter = 0;
    de_each(r, iter_ecs_counter, &counter);
    if (counter) {
        printf("test_destroy_one: counter %d\n", counter);
    }
    munit_assert_int(counter, ==, 0);

    de_ecs_free(r);
    return MUNIT_OK;
}
*/

static MunitResult test_destroy_one(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    int entts_num = 40;
    de_entity entts[entts_num];
    memset(entts, 0, sizeof(entts[0]) * entts_num);

    const int cycles = 10;

    const static de_cp_type comp = {
        .cp_id = 0,
        .cp_sizeof = sizeof(int),
        .initial_cap = 1000,
        .name = "main component",
    };

    for (int j = 0; j < cycles; j++) {
        for (int i = 0; i < entts_num; i++) {
            entts[i] = de_create(r);
            munit_assert_uint32(entts[i], !=, de_null);
            int *comp_value = de_emplace(r, entts[i], comp);
            munit_assert_ptr_not_null(comp_value);
            *comp_value = i;
        }
        for (int i = 0; i < entts_num; i++) {
            de_destroy(r, entts[i]);
        }
    }

    int counter = 0;
    de_each(r, iter_ecs_counter, &counter);
    if (counter) {
        printf("test_destroy_one: counter %d\n", counter);
    }
    munit_assert_int(counter, ==, 0);

    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitResult test_destroy_zero(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    int entts_num = 400;
    de_entity entts[entts_num];
    memset(entts, 0, sizeof(entts[0]) * entts_num);

    const int cycles = 1000;

    for (int j = 0; j < cycles; j++) {
        for (int i = 0; i < entts_num; i++) {
            entts[i] = de_create(r);
            munit_assert_uint32(entts[i], !=, de_null);
        }
        for (int i = 0; i < entts_num; i++) {
            de_destroy(r, entts[i]);
        }
    }

    int counter = 0;
    de_each(r, iter_ecs_counter, &counter);
    munit_assert_int(counter, ==, 0);

    de_ecs_free(r);
    return MUNIT_OK;
}

static TestDestroyCtx facade_create() {
    TestDestroyCtx ctx = {
        .r = de_ecs_new(),
        .map_entt2Desc = htable_new(&(HTableSetup) {
            .f_on_remove = map_on_remove,
        }),
    };

    return ctx;
}

static void facade_shutdown(TestDestroyCtx ctx) {
    HTableIterator i = htable_iter_new(ctx.map_entt2Desc);

    for (; htable_iter_valid(&i); htable_iter_next(&i)) {
        EntityDesc *ed = htable_iter_value(&i, NULL);
        assert(ed);

        for (int j = 0; j < ed->components_num; j++) {
            // Кто владеет памятью?
            if (ed->components[j]) {
                free(ed->components[j]);
                ed->components[j] = NULL;
            }
        }
    }

    htable_free(ctx.map_entt2Desc);
    de_ecs_free(ctx.r);
}

static void _test_destroy(de_cp_type comps[3]) {

    de_cp_type  comp1 = comps[0],
                comp2 = comps[1],
                comp3 = comps[2];

    for (int i = 0; i < 3; i++) {
        /*de_cp_type c = comps[i];*/
        /*de_cp_type_print(c);*/
        printf("\n");
    }

    TestDestroyCtx ctx = facade_create();
    ctx.types[0] = comp1;
    ctx.types[1] = comp2;
    ctx.types[2] = comp3;

    printf("\n");

    int entities_num = 10;
    int cycles = 1;

    for (int i = 0; i < cycles; ++i) {

        // создать сущности и прикрепить к ней случайное число компонент
        for (int j = 0; j < entities_num; j++) {
            de_entity ret = de_null;
            ctx = facade_ennt_create(ctx, &ret);
            munit_assert(ret != de_null);
        }

        // удалить одну случайную сущность целиком
        ctx = facade_entt_destroy(ctx, false);

        printf("ctx.map_entt2Desc %ld\n", htable_count(ctx.map_entt2Desc));

        // проверить, что состояние ecs соответствует ожидаемому, которое 
        // хранится в хэштаблицах.
        ctx = facade_compare_with_ecs(ctx);

    }

    facade_shutdown(ctx);
}

/*
    Задача - протестировать уничтожение сущностей вместе со всеми связанными
    компонентами.
    --
    Создается определенное количество сущностей, к каждой крепится от одного
    до 3х компонент.
    --
    Случайным образом удаляются несколько сущностей.
    Случайным образом добавляются несколько сущностей.
    --
    Проверка, что состояние ecs контейнера соответствует ожидаемому.
    Проверка происходит через de_view c одним компонентом
 */
static MunitResult test_create_emplace_destroy(
    const MunitParameter params[], void* data
) {
    printf("de_null %u\n", de_null);
    //srand(time(NULL));

    /*struct StrSet *set = strset_new();*/
    /*struct koh_Set *set_ecs = set_new();*/



    de_cp_type setups[][3] = {
        // {{{

        {
            {
                .cp_id = 0,
                .cp_sizeof = sizeof(int),
                .initial_cap = 10000,
                .name = "comp1",
            },
            {
                .cp_id = 1,
                .cp_sizeof = sizeof(int),
                .initial_cap = 10000,
                .name = "comp2",
            },
            {
                .cp_id = 2,
                .cp_sizeof = sizeof(int),
                .initial_cap = 10000,
                .name = "comp3",
            }
        },


        {
            {
                .cp_id = 0,
                .cp_sizeof = sizeof(int64_t),
                .initial_cap = 10,
                .name = "comp1",
            },
            {
                .cp_id = 1,
                .cp_sizeof = sizeof(int64_t),
                .initial_cap = 10,
                .name = "comp2",
            },
            {
                .cp_id = 2,
                .cp_sizeof = sizeof(int64_t),
                .initial_cap = 10,
                .name = "comp3",
            }
        },


        /*
        {
            {
                .cp_id = 0,
                .cp_sizeof = sizeof(char) + sizeof(char),
                .initial_cap = 10000,
                .name = "comp1",
            },
            {
                .cp_id = 1,
                .cp_sizeof = sizeof(char) + sizeof(char),
                .initial_cap = 10000,
                .name = "comp2",
            },
            {
                .cp_id = 2,
                .cp_sizeof = sizeof(char) + sizeof(char),
                .initial_cap = 10000,
                .name = "comp3",
            }
        },
        
        //*/

        /*

        {
            {
                .cp_id = 0,
                .cp_sizeof = sizeof(char),
                .initial_cap = 1,
                .name = "comp1",
            },
            {
                .cp_id = 1,
                .cp_sizeof = sizeof(char),
                .initial_cap = 1,
                .name = "comp2",
            },
            {
                .cp_id = 2,
                .cp_sizeof = sizeof(char),
                .initial_cap = 1,
                .name = "comp3",
            }
        },

        // */

        // }}}
    };
    int setups_num = sizeof(setups) / sizeof(setups[0]);

    printf("setups_num %d\n", setups_num);
    for (int j = 0; j < setups_num; j++) {
        _test_destroy(setups[j]);
    }


    return MUNIT_OK;
}

static MunitResult test_emplace_destroy(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    de_entity entts[1000] = {0};
    int entts_num = 0;

    de_cp_type types[] = { cp_cell }; 
    size_t types_num = sizeof(types) / sizeof(types[0]);

    for (int k = 0; k < 3; k++) {

        for (int x = 0; x < 50; x++) {
            for (int y = 0; y < 50; y++) {
                struct Cell *cell = create_cell(r, x, y, NULL);
                munit_assert_ptr_not_null(cell);
            }
        }

        for (int j = 0; j < 3; j++) {

            for (int i = 0; i < 5; i++) {

                for (de_view v = de_view_create(r, types_num, types);
                        de_view_valid(&v); de_view_next(&v)) {

                    munit_assert(de_valid(r, de_view_entity(&v)));
                    struct Cell *c = de_view_get_safe(&v, cp_cell);
                    munit_assert_ptr_not_null(c);

                    c->moving = false;
                    c->from_x = rand() % 100 + 10;
                    c->from_y = rand() % 100 + 10;
                }

            }

            for (de_view v = de_view_create(r, types_num, types);
                    de_view_valid(&v); de_view_next(&v)) {

                munit_assert(de_valid(r, de_view_entity(&v)));
                struct Cell *c = de_view_get_safe(&v, cp_cell);
                munit_assert_ptr_not_null(c);

                if (c->from_x == 10 || c->from_y == 10) {
                    if (verbose_print) 
                        printf("removing entity\n");
                    de_destroy(r, de_view_entity(&v));
                } else {
                    if (entts_num < sizeof(entts) / sizeof(entts[0])) {
                        entts[entts_num++] = de_view_entity(&v);
                    }
                }
            }

        }
    }

    /*
    for (int i = 0; i < entts_num; ++i) {
        if (de_valid(r, entts[i])) {
            munit_assert(de_has(r, entts[i], cp_cell));
            de_destroy(r, entts[i]);
        }
    }
    */

    for (de_view v = de_view_create(r, types_num, types);
        de_view_valid(&v); de_view_next(&v)) {

        munit_assert(de_valid(r, de_view_entity(&v)));
        struct Cell *c = de_view_get_safe(&v, cp_cell);
        munit_assert_ptr_not_null(c);

        munit_assert_int(c->from_x, >=, 10);
        munit_assert_int(c->from_x, <=, 10 + 100);
        munit_assert_int(c->from_y, >=, 10);
        munit_assert_int(c->from_y, <=, 10 + 100);
        munit_assert(c->moving == false);
    }

    de_ecs_free(r);
    return MUNIT_OK;
}

static MunitResult test_has(
    const MunitParameter params[], void* data
) {

    {
        de_ecs *r = de_ecs_new();
        de_entity e = de_create(r);
        de_emplace(r, e, cp_triple);
        //memset(tr, 1, sizeof(*tr));
        munit_assert(de_has(r, e, cp_triple) == true);
        munit_assert(de_has(r, e, cp_cell) == false);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_entity e = de_create(r);
        de_emplace(r, e, cp_triple);
        de_emplace(r, e, cp_cell);
        //memset(tr, 1, sizeof(*tr));
        munit_assert(de_has(r, e, cp_triple) == true);
        munit_assert(de_has(r, e, cp_cell) == true);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();
        de_entity e = de_create(r);
        de_emplace(r, e, cp_triple);
        de_emplace(r, e, cp_cell);
        //memset(tr, 1, sizeof(*tr));
        munit_assert(de_has(r, e, cp_triple) == true);
        munit_assert(de_has(r, e, cp_cell) == true);
        de_ecs_free(r);
    }

    {
        de_ecs *r = de_ecs_new();

        if (verbose_print)
            de_ecs_print(r);
        de_entity e1 = de_create(r);
        de_emplace(r, e1, cp_triple);

        if (verbose_print)
            de_ecs_print(r);
        de_entity e2 = de_create(r);
        de_emplace(r, e2, cp_cell);

        if (verbose_print)
            de_ecs_print(r);
        de_entity e3 = de_create(r);
        de_emplace(r, e3, cp_node);

        if (verbose_print)
            de_ecs_print(r);

        if (verbose_print) {
            de_storage_print(r, cp_triple);
            de_storage_print(r, cp_cell);
            de_storage_print(r, cp_node);
        }

        munit_assert(de_has(r, e1, cp_triple) == true);
        munit_assert(de_has(r, e1, cp_cell) == false);
        munit_assert(de_has(r, e2, cp_triple) == false);
        munit_assert(de_has(r, e2, cp_cell) == true);

        //de_destroy(r, e1);
        de_remove_all(r, e2);
        if (verbose_print)
            de_ecs_print(r);
        de_remove_all(r, e1);
        if (verbose_print)
            de_ecs_print(r);
        de_remove_all(r, e3);
        if (verbose_print)
            de_ecs_print(r);
        //de_destroy(r, e2);

        if (verbose_print) {
            de_storage_print(r, cp_triple);
            de_storage_print(r, cp_cell);
            de_storage_print(r, cp_node);
        }

        munit_assert(de_valid(r, e1));
        munit_assert(de_valid(r, e2));
        munit_assert(de_valid(r, e3));
        munit_assert((e1 != e2) && (e1 != e3));

        if (verbose_print)
            de_ecs_print(r);
        
        munit_assert(de_orphan(r, e1) == true);
        munit_assert(de_orphan(r, e2) == true);
        munit_assert(de_orphan(r, e3) == true);

        if (verbose_print)
            de_ecs_print(r);

        munit_assert(de_has(r, e1, cp_triple) == false);
        //munit_assert(de_has(r, e2, cp_cell) == false);
        //munit_assert(de_has(r, e3, cp_node) == false);

        de_ecs_free(r);
    }

    // XXX: Что если к одной сущности несколько раз цеплять компонент одного и
    // того же типа?
    {
        de_ecs *r = de_ecs_new();
        const int num = 100;
        de_entity ennts[num];
        for (int i = 0; i < num; i++) {
            de_entity e = de_create(r);
            de_emplace(r, e, cp_triple);
            ennts[i] = e;
        }

        for (int i = 0; i < num; i++) {
            de_entity e = ennts[i];
            munit_assert(de_has(r, e, cp_triple) == true);
            munit_assert(de_has(r, e, cp_cell) == false);
        }

        de_view_single v = de_view_single_create(r, cp_triple);
        for(; de_view_single_valid(&v); de_view_single_next(&v)) {
            de_entity e = de_view_single_entity(&v);
            munit_assert(de_has(r, e, cp_triple) == true);
            munit_assert(de_has(r, e, cp_cell) == false);
        }

        de_ecs_free(r);
    }



    return MUNIT_OK;
}

/*
static HTableAction iter_table_cell(
    const void *key, int key_len, void *value, int value_len, void *udata
) {
    de_entity e = *(de_entity*)key;
    printf("iter_table_cell: e %u\n", e);
    return HTABLE_ACTION_NEXT;
}
*/

static MunitResult test_view_get(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    size_t total_num = 1000;
    size_t idx = 0;
    size_t cell_num = 100;
    size_t triple_cell_num = 600;
    size_t triple_num = 300;
    munit_assert(total_num == triple_num + cell_num + triple_cell_num);

    de_entity *ennts_all = calloc(total_num, sizeof(de_entity));

    HTable *table_cell = htable_new(NULL);
    HTable *table_triple = htable_new(NULL);
    HTable *table_triple_cell = htable_new(NULL);

    // Создать случайное количество сущностей.
    // Проход при помощи de_view_single

    // Часть сущностей с компонентом cp_cell
    for (int i = 0; i < cell_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);
        struct Cell *cell = de_emplace(r, e, cp_cell);
        // {{{
        cell->from_x = rand() % 1000;
        cell->from_y = rand() % 1000;
        cell->to_x = rand() % 1000;
        cell->to_y = rand() % 1000;
        cell->value = rand() % 1000;
        // }}}
        htable_add(table_cell, &e, sizeof(e), cell, sizeof(*cell));
    }

    printf("--------------------------\n");
    //htable_each(table_cell, iter_table_cell, NULL);
    printf("table_cell count %zu\n", htable_count(table_cell));
    printf("--------------------------\n");

    struct Couple {
        struct Cell     cell;
        struct Triple   triple;
    };

    // Часть сущностей с компонентами cp_cell и cp_triple
    for (int i = 0; i < triple_cell_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);

        struct Triple *triple = de_emplace(r, e, cp_triple);
        // {{{
        triple->dx = rand() % 1000;
        triple->dy = rand() % 1000;
        triple->dz = rand() % 1000;
        // }}}
        struct Cell *cell = de_emplace(r, e, cp_cell);
        // {{{
        cell->from_x = rand() % 1000;
        cell->from_y = rand() % 1000;
        cell->to_x = rand() % 1000;
        cell->to_y = rand() % 1000;
        cell->value = rand() % 1000;
        // }}}
        
        struct Couple x = {
            .cell = *cell,
            .triple = *triple,
        };

        htable_add(table_triple_cell, &e, sizeof(e), &x, sizeof(x));
    }

    // Часть сущностей с компонентом cp_triple
    for (int i = 0; i < triple_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);

        struct Triple *triple = de_emplace(r, e, cp_triple);
        // {{{
        triple->dx = rand() % 1000;
        triple->dy = rand() % 1000;
        triple->dz = rand() % 1000;
        // }}}
        htable_add(table_triple, &e, sizeof(e), triple, sizeof(*triple));
    }

    /*
    if (verbose_print)
        htable_print(table_triple);
        */

    {
        de_view v = de_view_create(r, 1, (de_cp_type[]){cp_cell});
        for (; de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Cell *cell1 = de_view_get(&v, cp_cell);
            size_t sz = sizeof(e);
            //printf("e %u\n", e);
            const struct Cell *cell2 = htable_get(table_cell, &e, sz, NULL);

            // Обработка cp_triple + cp_cell
            munit_assert_not_null(cell1);
            if (!cell2) {
                munit_assert(de_has(r, e, cp_triple));
            } else {
                //munit_assert(de_has(r, e, cp_triple));
                munit_assert(!memcmp(cell1, cell2, sizeof(*cell1)));
            }
        }
    }
    // */

    {
        de_view v = de_view_create(r, 1, (de_cp_type[]){cp_triple});
        for (;de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Triple *tr1 = de_view_get(&v, cp_triple);
            size_t sz = sizeof(e);
            const struct Triple *tr2 = htable_get(table_triple, &e, sz, NULL);
            munit_assert_not_null(tr1);

            // Обработка cp_triple + cp_cell
            if (!tr2) {
                munit_assert(de_has(r, e, cp_triple));
            } else {
                munit_assert(!memcmp(tr1, tr2, sizeof(*tr1)));
            }

        }
    }

    // Часть сущностей с компонентами cp_cell и cp_triple
    {
        de_view v = de_view_create(r, 2, (de_cp_type[]){cp_triple, cp_cell});
        for (;de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Triple *tr = de_view_get(&v, cp_triple);
            const struct Triple *cell = de_view_get(&v, cp_cell);
            size_t sz = sizeof(e);

            const struct Couple *x = htable_get(
                table_triple_cell, &e, sz, NULL
            );

            //munit_assert_not_null(tr);
            //munit_assert_not_null(cell);
            if (tr && cell) {
                munit_assert_not_null(x);
                munit_assert(!memcmp(tr, &x->triple, sizeof(*tr)));
                munit_assert(!memcmp(cell, &x->cell, sizeof(*cell)));
            } else if (!tr) {
                munit_assert(de_has(r, e, cp_cell));
            } else if (!cell) {
                munit_assert(de_has(r, e, cp_triple));
            }
        }
    }
    htable_free(table_cell);
    htable_free(table_triple);
    htable_free(table_triple_cell);
    free(ennts_all);
    //free(ennts_triple);
    //free(ennts_cell);
    //free(ennts_triple_cell);
    de_ecs_free(r);
    return MUNIT_OK;
}

// TODO: Написать более простой тест de_view_single
static MunitResult test_view_single_get(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    size_t total_num = 1000;
    size_t idx = 0;
    size_t cell_num = 100;
    size_t triple_cell_num = 600;
    size_t triple_num = 300;
    munit_assert(total_num == triple_num + cell_num + triple_cell_num);

    de_entity *ennts_all = calloc(total_num, sizeof(de_entity));

    HTable *table_cell = htable_new(NULL);
    HTable *table_triple = htable_new(NULL);

    // Создать случайное количество сущностей.
    // Проход при помощи de_view_single

    // Часть сущностей с компонентом cp_cell
    for (int i = 0; i < cell_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);
        struct Cell *cell = de_emplace(r, e, cp_cell);
        // {{{
        cell->from_x = rand() % 1000;
        cell->from_y = rand() % 1000;
        cell->to_x = rand() % 1000;
        cell->to_y = rand() % 1000;
        cell->value = rand() % 1000;
        // }}}
        htable_add(table_cell, &e, sizeof(e), cell, sizeof(*cell));
    }

    printf("--------------------------\n");
    //htable_each(table_cell, iter_table_cell, NULL);
    printf("table_cell count %zu\n", htable_count(table_cell));
    printf("--------------------------\n");

    struct Couple {
        struct Cell     cell;
        struct Triple   triple;
    };

    // Часть сущностей с компонентами cp_cell и cp_triple
    for (int i = 0; i < triple_cell_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);

        struct Triple *triple = de_emplace(r, e, cp_triple);
        // {{{
        triple->dx = rand() % 1000;
        triple->dy = rand() % 1000;
        triple->dz = rand() % 1000;
        // }}}
        struct Cell *cell = de_emplace(r, e, cp_cell);
        // {{{
        cell->from_x = rand() % 1000;
        cell->from_y = rand() % 1000;
        cell->to_x = rand() % 1000;
        cell->to_y = rand() % 1000;
        cell->value = rand() % 1000;
        // }}}
        
        /*
        struct Couple x = {
            .cell = *cell,
            .triple = *triple,
        };
        */

        //htable_add(table_triple_cell, &e, sizeof(e), &x, sizeof(x));
    }

    // Часть сущностей с компонентом cp_triple
    for (int i = 0; i < triple_num; ++i) {
        de_entity e = ennts_all[idx++] = de_create(r);

        struct Triple *triple = de_emplace(r, e, cp_triple);
        // {{{
        triple->dx = rand() % 1000;
        triple->dy = rand() % 1000;
        triple->dz = rand() % 1000;
        // }}}
        htable_add(table_triple, &e, sizeof(e), triple, sizeof(*triple));
    }

    /*
    if (verbose_print)
        htable_print(table_triple);
        */

    {
        de_view v = de_view_create(r, 1, (de_cp_type[]){cp_cell});
        for (; de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Cell *cell1 = de_view_get(&v, cp_cell);
            size_t sz = sizeof(e);
            //printf("e %u\n", e);
            const struct Cell *cell2 = htable_get(table_cell, &e, sz, NULL);

            // Обработка cp_triple + cp_cell
            munit_assert_not_null(cell1);
            if (!cell2) {
                munit_assert(de_has(r, e, cp_triple));
            } else {
                //munit_assert(de_has(r, e, cp_triple));
                munit_assert(!memcmp(cell1, cell2, sizeof(*cell1)));
            }
        }
    }
    // */

    {
        de_view v = de_view_create(r, 1, (de_cp_type[]){cp_triple});
        for (;de_view_valid(&v); de_view_next(&v)) {
            de_entity e = de_view_entity(&v);
            const struct Triple *tr1 = de_view_get(&v, cp_triple);
            size_t sz = sizeof(e);
            const struct Triple *tr2 = htable_get(table_triple, &e, sz, NULL);
            munit_assert_not_null(tr1);

            // Обработка cp_triple + cp_cell
            if (!tr2) {
                munit_assert(de_has(r, e, cp_triple));
            } else {
                munit_assert(!memcmp(tr1, tr2, sizeof(*tr1)));
            }

        }
    }

    htable_free(table_cell);
    htable_free(table_triple);
    free(ennts_all);
    de_ecs_free(r);
    return MUNIT_OK;
}

// проверка на добавление содержимого и удаление содержимого
// проверка de_get()
static MunitResult test_sparse_ecs(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    de_entity e1, e2, e3;

    e1 = de_create(r);
    de_emplace(r, e1, cp_cell);
    munit_assert_not_null(de_get(r, e1, cp_cell));
    de_destroy(r, e1);
    munit_assert(!de_valid(r, e1));

    e2 = de_create(r);
    de_emplace(r, e2, cp_cell);
    munit_assert_not_null(de_get(r, e2, cp_cell));
    de_destroy(r, e2);
    munit_assert(!de_valid(r, e2));

    e3 = de_create(r);
    de_emplace(r, e3, cp_cell);
    munit_assert_not_null(de_get(r, e3, cp_cell));
    de_destroy(r, e3);
    munit_assert(!de_valid(r, e3));

    de_ecs_free(r);

    return MUNIT_OK;
}

static MunitResult test_sparse_1(
    const MunitParameter params[], void* data
) {

    // добавить
    {
        de_sparse s = {};
        de_entity e1, e2, e3;

        e1 = de_make_entity(de_entity_id(100), de_entity_ver(0));
        e2 = de_make_entity(de_entity_id(10), de_entity_ver(0));
        e3 = de_make_entity(de_entity_id(11), de_entity_ver(0));

        de_sparse_init(&s, 10);

        de_sparse_emplace(&s, e1);
        munit_assert(de_sparse_contains(&s, e1));

        de_sparse_emplace(&s, e2);
        munit_assert(de_sparse_contains(&s, e2));

        de_sparse_emplace(&s, e3);
        munit_assert(de_sparse_contains(&s, e3));

        de_sparse_destroy(&s);
    }

    // добавить и удалить
    {
        de_sparse s = {};
        de_entity e1, e2, e3;

        e1 = de_make_entity(de_entity_id(100), de_entity_ver(0));
        e2 = de_make_entity(de_entity_id(10), de_entity_ver(0));
        e3 = de_make_entity(de_entity_id(11), de_entity_ver(0));

        de_sparse_init(&s, 1);

        de_sparse_emplace(&s, e1);
        munit_assert(de_sparse_contains(&s, e1));

        de_sparse_emplace(&s, e2);
        munit_assert(de_sparse_contains(&s, e2));

        de_sparse_emplace(&s, e3);
        munit_assert(de_sparse_contains(&s, e3));

        de_sparse_remove(&s, e1);
        munit_assert(!de_sparse_contains(&s, e1));

        de_sparse_remove(&s, e2);
        munit_assert(!de_sparse_contains(&s, e2));

        de_sparse_remove(&s, e3);
        munit_assert(!de_sparse_contains(&s, e3));

        de_sparse_destroy(&s);
    }

    return MUNIT_OK;
}

// XXX: Что проверяет данный код?
static MunitResult test_sparse_2_non_seq_idx(
    const MunitParameter params[], void* data
) {
    // проверка на добавление и удаление содержимого
    de_sparse s = {};
    de_entity e1, e2, e3;

    e1 = de_make_entity(de_entity_id(1), de_entity_ver(0));
    e2 = de_make_entity(de_entity_id(0), de_entity_ver(0));
    e3 = de_make_entity(de_entity_id(3), de_entity_ver(0));

    de_sparse_init(&s, 1);

    // Проверка, можно-ли добавлять и удалять циклически
    for (int i = 0; i < 10; i++) {
        //printf("test_sparse_2:%s\n", de_sparse_contains(&s, e1) ? "true" : "false");
        de_sparse_emplace(&s, e1);
        de_sparse_emplace(&s, e2);
        de_sparse_emplace(&s, e3);

        de_sparse_remove(&s, e1);
        /*de_sparse_remove(&s, e1);*/
        /*de_sparse_remove(&s, e1);*/

        // XXX: Почему элемент не удаляется?
        //munit_assert(!de_sparse_contains(&s, e1));
        //printf("test_sparse_2:%s\n", de_sparse_contains(&s, e1) ? "true" : "false");
        munit_assert(!de_sparse_contains(&s, e1));

        de_sparse_remove(&s, e2);
        //munit_assert(!de_sparse_contains(&s, e2));
        munit_assert(!de_sparse_contains(&s, e2));

        de_sparse_remove(&s, e3);
        //munit_assert(!de_sparse_contains(&s, e3));
        munit_assert(!de_sparse_contains(&s, e3));
    }

    de_sparse_destroy(&s);

    return MUNIT_OK;
}

static MunitResult test_sparse_2(
    const MunitParameter params[], void* data
) {
    // проверка на добавление и удаление содержимого
    de_sparse s = {};
    de_entity e1, e2, e3;

    e1 = de_make_entity(de_entity_id(1), de_entity_ver(0));
    e2 = de_make_entity(de_entity_id(2), de_entity_ver(0));
    e3 = de_make_entity(de_entity_id(3), de_entity_ver(0));

    de_sparse_init(&s, 1);

    // Проверка, можно-ли добавлять и удалять циклически
    for (int i = 0; i < 10; i++) {
        //printf("test_sparse_2:%s\n", de_sparse_contains(&s, e1) ? "true" : "false");
        de_sparse_emplace(&s, e1);
        de_sparse_emplace(&s, e2);
        de_sparse_emplace(&s, e3);

        //printf("\ntest_sparse_2: de_sparse_index %zu\n", de_sparse_index(&s, e1));
        //munit_assert(de_sparse_index(&s, e1))
        de_sparse_remove(&s, e1);
        //de_sparse_remove(&s, e1);
        //de_sparse_remove(&s, e1);

        // XXX: Почему элемент не удаляется?
        //munit_assert(!de_sparse_contains(&s, e1));
        //printf("test_sparse_2: de_sparse_index %zu\n", de_sparse_index(&s, e1));
        //printf("test_sparse_2:%s\n", de_sparse_contains(&s, e1) ? "true" : "false");

        munit_assert(!de_sparse_contains(&s, e1));

        de_sparse_remove(&s, e2);
        //munit_assert(!de_sparse_contains(&s, e2));
        munit_assert(!de_sparse_contains(&s, e2));

        de_sparse_remove(&s, e3);
        //munit_assert(!de_sparse_contains(&s, e3));
        munit_assert(!de_sparse_contains(&s, e3));
    }

    de_sparse_destroy(&s);

    return MUNIT_OK;
}

static MunitResult test_emplace_1_insert(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    xorshift32_state rng = xorshift32_init();
    de_ecs_register(r, cp_type_1);

    /*const int passes = 10000;*/
    // XXX: Что за проходы, за что они отвечают?
    const int passes = 10;

    HTable  *set_c = htable_new(NULL), // компоненты
            *set_e = htable_new(NULL); // сущности

    for (int i = 0; i < passes; ++i) {
        // создать сущность
        de_entity entt = de_create(r);
        htable_add(set_e, &entt, sizeof(entt), NULL, 0);

        Comp_1 comp = {};
        Comp_1_new(&rng, entt, &comp);

        // Добавить компонент к сущности
        Comp_1 *c = de_emplace(r, entt, cp_type_1);
        assert(c);

        *c = comp; // записал значение компоненты в ecs
        htable_add(set_c, c, sizeof(*c), NULL, 0);
    }

    // Пройтись по всем сущностям
    for (de_view_single view = de_view_single_create(r, cp_type_1);
         de_view_single_valid(&view);
         de_view_single_next(&view)) {

        de_entity e = de_view_single_entity(&view);
        // Наличие сущности в таблице
        munit_assert(htable_exist(set_e, &e, sizeof(e)) == true);
        Comp_1 *c = de_view_single_get(&view);
        // Наличие компонента в таблице
        munit_assert(htable_exist(set_c, c, sizeof(*c)) == true);
    }

    htable_free(set_c);
    htable_free(set_e);
    de_ecs_free(r);
    return MUNIT_OK;
}

// Проверка создания сущности, добавления к ней компоненты, итерация
static MunitResult test_emplace_1_insert_remove(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();

    xorshift32_state rng = xorshift32_init();
    de_ecs_register(r, cp_type_1);

    /*const int passes = 10000;*/
    // XXX: Что за проходы, за что они отвечают?
    const int passes = 10;

    HTable  *set_c = htable_new(NULL), // компоненты
            *set_e = htable_new(NULL); // сущности

    for (int i = 0; i < passes; ++i) {
        // создать сущность
        de_entity entt = de_create(r);

        Comp_1 comp = {};
        Comp_1_new(&rng, entt, &comp);

        // Добавить компонент
        Comp_1 *c = de_emplace(r, entt, cp_type_1);
        assert(c);

        *c = comp;
        htable_add(set_c, c, sizeof(*c), NULL, 0);
        htable_add(set_e, &entt, sizeof(entt), NULL, 0);
    }

    // Пройтись по всем сущностям
    for (de_view_single view = de_view_single_create(r, cp_type_1);
         de_view_single_valid(&view);
         de_view_single_next(&view)) {

        de_entity e = de_view_single_entity(&view);
        // Наличие сущности в таблице
        munit_assert(htable_exist(set_e, &e, sizeof(e)) == true);
        Comp_1 *c = de_view_single_get(&view);
        // Наличие компонента в таблице
        munit_assert(htable_exist(set_c, c, sizeof(*c)) == true);
    }

    HTableIterator i = htable_iter_new(set_e);
    // Пройтись по всем сущностям
    for (; htable_iter_valid(&i); htable_iter_next(&i)) {
        de_entity e = de_null;
        void *key = htable_iter_key(&i, NULL);
        e = *((de_entity*)key);
        de_remove(r, e, cp_type_1);
    }

    for (de_view_single view = de_view_single_create(r, cp_type_1);
         de_view_single_valid(&view);
         de_view_single_next(&view)) {
        // В цикл нет захода
        munit_assert(true);
    }

    i = htable_iter_new(set_e);
    // Пройтись по всем сущностям
    for (; htable_iter_valid(&i); htable_iter_next(&i)) {
        de_entity e = de_null;
        void *key = htable_iter_key(&i, NULL);
        e = *((de_entity*)key);

        munit_assert(!de_has(r, e, cp_type_1));
    }


    htable_free(set_c);
    htable_free(set_e);
    de_ecs_free(r);
    return MUNIT_OK;
}

// Создать и удалить
static MunitResult test_new_free(
    const MunitParameter params[], void* data
) {
    de_ecs *r = de_ecs_new();
    de_ecs_free(r);
    return MUNIT_OK;
}

void _test_ecs_each_iter(int num) {
    assert(num >= 0);
    de_ecs *r = de_ecs_new();

    HTable *t = htable_new(NULL);

    for (int i = 0; i < num; i++) {
        // Создать сущность
        de_entity e = de_create(r);
        bool val = false;
        // Добавить в таблицу значение сущности и флаг ее просмотра
        htable_add(t, &e, sizeof(e), &val, sizeof(val));
    }

    // Счетчик итераций
    int cnt = 0;
    // Проверка итератора
    for (de_each_iter i = de_each_begin(r);
        de_each_valid(&i); de_each_next(&i)) {
        de_entity e = de_each_entity(&i);
        // Сущность должна существовать
        munit_assert(e != de_null);
        // Флаг просмотра
        bool *val = htable_get(t, &e, sizeof(e), NULL);
        // Флаг должен существовать
        munit_assert_not_null(val);
        // Флаг не должен быть установлен
        munit_assert(*val == false);
        // Установка флага
        *val = true;
        cnt++;
    }

    HTableIterator i = htable_iter_new(t);
    // Проверка работы de_each_iter
    for (; htable_iter_valid(&i); htable_iter_next(&i)) {
        de_entity *e = htable_iter_key(&i, NULL);
        munit_assert_not_null(e);
        munit_assert(*e != de_null);
        bool *val = NULL;
        val = htable_iter_value(&i, NULL);
        munit_assert_not_null(val);
        // Главное - флаг должен быть установлен
        munit_assert(*val == true);
    }

    printf("test_ecs_each_iter: cnt %d, num %d\n", cnt, num);
    // Дополнительная проверка через счетчик
    munit_assert(cnt == num);

    htable_free(t);
    de_ecs_free(r);
}

// Проверка итерации по сущностям
static MunitResult test_ecs_each_iter(
    const MunitParameter params[], void* data
) {
    _test_ecs_each_iter(0);
    _test_ecs_each_iter(10);
    _test_ecs_each_iter(1000);
    return MUNIT_OK;
}


static MunitTest test_suite_tests[] = {

    {
      (char*) "/ecs_each_iter",
      test_ecs_each_iter,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

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

    {
      (char*) "/sparse_ecs",
      test_sparse_ecs,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },


    {
      (char*) "/sparse_1",
      test_sparse_1,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    {
      (char*) "/test_sparse_2_non_seq_idx",
      test_sparse_2_non_seq_idx,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },


    {
      (char*) "/sparse_2",
      test_sparse_2,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },


    // FIXME:
    {
      (char*) "/has",
      test_has,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },
    // */

    // FIXME:
    {
      (char*) "/view_get",
      test_view_get,
      NULL,
      NULL,
      MUNIT_TEST_OPTION_NONE,
      NULL
    },

    // FIXME:
      {
        (char*) "/view_single_get",
        test_view_single_get,
        NULL,
        NULL,
        MUNIT_TEST_OPTION_NONE,
        NULL
      },
    // */

  {
    (char*) "/try_get_none_existing_component",
    test_try_get_none_existing_component,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  /*
  {
    (char*) "/destroy_one_random",
    test_destroy_one_random,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  */

  {
    (char*) "/destroy_one",
    test_destroy_one,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  {
    (char*) "/destroy_zero",
    test_destroy_zero,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

   //FIXME:
  {
    (char*) "/destroy",
    test_create_emplace_destroy,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },
  // */

  {
    (char*) "/emplace_destroy",
    test_emplace_destroy,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  {
    (char*) "/ecs_clone_mono",
    test_ecs_clone_mono,
    NULL,
    NULL,
    MUNIT_TEST_OPTION_NONE,
    NULL
  },

  {
    (char*) "/ecs_clone_multi",
    test_ecs_clone_multi,
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

    // Регистрация компонентов для примера
    components[components_num++] = cp_type_1;
    components[components_num++] = cp_type_5;
    components[components_num++] = cp_type_17;
    components[components_num++] = cp_type_73;

    return munit_suite_main(&test_suite, (void*) "µnit", argc, argv);
}
