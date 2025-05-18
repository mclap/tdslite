cmake_minimum_required(VERSION 3.23)
project(tdslite)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TDS_BUILD_APPS "Build programs (cli etc)" OFF)
option(TDS_BUILD_TESTS "Build test programs" OFF)

find_package(Iconv REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE DEBUG)
endif()

if (TDS_BUILD_TESTS)

find_package(GTest REQUIRED)
include(GoogleTest)
enable_testing()

function(tds_add_tests LIBNAME GLOBEXPR)
    file(GLOB_RECURSE source_tests ${GLOBEXPR})

    add_executable(${LIBNAME}-tests EXCLUDE_FROM_ALL
        ${source_tests}
        )
    target_link_libraries(${LIBNAME}-tests
        ${LIBNAME}
        GTest::gtest_main
        )

    gtest_discover_tests(${LIBNAME}-tests)
endfunction()

else() # TDS_BUILD_TESTS

function(tds_add_tests)
endfunction()

endif() # TDS_BUILD_TESTS

add_subdirectory(libtdslite)
add_subdirectory(cli)
