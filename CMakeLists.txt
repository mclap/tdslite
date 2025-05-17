cmake_minimum_required(VERSION 3.23)
project(tdslite)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

find_package(Iconv REQUIRED)
find_package(GTest REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE DEBUG)
endif()

add_subdirectory(libtdslite)
add_subdirectory(cli)
