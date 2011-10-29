cmake_minimum_required(VERSION 2.6)
project(tdslite)
enable_testing()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(ICONV_LIB "")
else(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	include_directories(/usr/local/include)
	find_library(ICONV_LIB NAMES iconv PATHS /usr/lib /usr/local/lib)
endif(CMAKE_SYSTEM_NAME STREQUAL "Linux")

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE DEBUG)
endif(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS_PROFILE        "-g3 -fno-inline -O0")
	set(CMAKE_C_FLAGS_PROFILE          "-g3 -fno-inline -O0")
endif (CMAKE_COMPILER_IS_GNUCXX)

add_library(tdslite
	net.c net.h
	debug.c debug.h
	tds_frames.cpp tds_frames.h
	tds_connection.cpp tds_connection.h
	)

add_executable(cli
	main.cpp
	)
target_link_libraries(cli tdslite ${ICONV_LIB})

set(check_PROGRAMS
	check-frame_login7
	check-frame_error
	check-frame_response_introw
	)

function(register_tests srcprefix names)
	foreach(cp ${names})
		add_executable(${cp} ${srcprefix}${cp}.cpp)
		target_link_libraries(${cp} tdslite ${ICONV_LIB})
		add_test(${cp} ${cp})
	endforeach(cp)
endfunction(register_tests)

register_tests(./ "${check_PROGRAMS}")
