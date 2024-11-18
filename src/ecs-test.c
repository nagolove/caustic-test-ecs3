// vim: set colorcolumn=85
// vim: fdm=marker

// Запускалка тестов de_ecs

#include "koh_destral_ecs.h"
#include "koh_hashers.h"

static const MunitSuite test_suite = {
  "de_ecs/", 
  NULL,
  &test_de_ecs_suite_internal,
  1, MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char **argv) {
    koh_hashers_init();
    de_ecs_test_init();
    return munit_suite_main(&test_suite, (void*) "µnit", argc, argv);
}
