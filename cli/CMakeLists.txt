add_executable(cli
	main.cpp
	)
target_link_libraries(cli tdslite ${ICONV_LIB})
