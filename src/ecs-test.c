// vim: set colorcolumn=85
// vim: fdm=marker

// Запускалка тестов e_ecs

#include "koh_ecs.h"
#include "koh_hashers.h"

static const MunitSuite test_suite = {
  "ecs/", 
  NULL,
  &test_e_suite_internal,
  1, MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char **argv) {
    koh_hashers_init();
    //e_test_init();
    return munit_suite_main(&test_suite, (void*) "µnit", argc, argv);
}
