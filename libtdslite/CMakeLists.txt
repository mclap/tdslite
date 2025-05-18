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

tds_add_tests(tdslite "tests/*.cpp")
