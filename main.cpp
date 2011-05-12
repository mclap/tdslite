#include <stdlib.h>
#include <stdio.h>

#include "tds_connection.h"

int main(int argc, char *argv[])
{
	if (argc < 6)
	{
		printf("usage: %s host port user pass query\n", argv[0]);
		return 1;
	}

	/*
	net_free(cn);
	*/
	tds::connection cn;
	cn.connect(argv[1], strtol(argv[2], 0, 10));
	cn.call_login7();

	return 0;
}
