cmake_minimum_required(VERSION 2.8.0)

if(NOT PROST_MAIN_CMAKELISTS_READ)
  message(
    FATAL_ERROR
    "Run cmake on the CMakeLists.txt in the 'src' directory, "
    "not the one in 'src/search'. Please delete CMakeCache.txt "
    "and CMakeFiles/ from the current directory and restart cmake.")
endif()

## == Project ==
project(prost)
prost_set_compiler_flags()
prost_set_linker_flags()

## == BDD ==
find_package(BDD REQUIRED)

## == Includes ==
include_directories("logical_expressions_includes")
include_directories("utils")
include_directories(${BDD_INCLUDE_DIRS})

## == Source Files ==
set(SEARCH_SOURCES
    action_selection
    backup_function
    depth_first_search
    evaluatables
    initializer
    ipc_client
    iterative_deepening_search
    logical_expressions
    minimal_lookahead_search
    outcome_selection
    parser
    probability_distribution
    prost_planner
    random_walk
    recommendation_function
    search_engine
    states
    thts
    uniform_evaluation_search
    utils/base64
    utils/logger
    utils/math_utils
    utils/random
    utils/stopwatch
    utils/string_utils
    utils/strxml
    utils/system_utils
)

## == Doctest ==
set(SEARCH_TEST_SOURCES
    ../doctest/doctest
    tests/evaluate_test
    tests/probability_distribution_test
)

# add unit test files in debug build
IF(CMAKE_BUILD_TYPE MATCHES Debug)
    set(SEARCH_SOURCES ${SEARCH_SOURCES} ${SEARCH_TEST_SOURCES})
ENDIF(CMAKE_BUILD_TYPE MATCHES Debug)

## == Add Executable ==
add_executable(search ${SEARCH_SOURCES} main)

## == Link ==
target_link_libraries(search ${BDD_LIBRARIES})
