file(GLOB_RECURSE sources
    "src/*.cpp"
    "src/*.c"
    )

add_library(tdslite
    ${sources}
    )

target_include_directories(tdslite PUBLIC
    include/
    )
target_link_libraries(tdslite PUBLIC
    Iconv::Iconv
    )

file(GLOB_RECURSE source_tests "tests/*.cpp")

add_executable(tdslite-tests EXCLUDE_FROM_ALL
    ${source_tests}
    )
target_link_libraries(tdslite-tests
    tdslite
    GTest::gtest_main
    )

include(GoogleTest)
gtest_discover_tests(tdslite-tests)
