if (NOT TDS_BUILD_APPS)
    return()
endif()

add_executable(cli
	main.cpp
	)
target_link_libraries(cli tdslite ${ICONV_LIB})
