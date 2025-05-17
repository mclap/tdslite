#include "tdslite/tds_connection.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    if (argc < 7)
    {
        printf("usage: %s host port user pass database query\n", argv[0]);
        return 1;
    }

    /*
    net_free(cn);
    */
    tds::connection cn;
    if (!cn.connect(argv[1], strtol(argv[2], 0, 10)))
    {
        printf("CONNECT - FAILED\n");
        return 1;
    }
    printf("CONNECT - OK\n");


#if 0
    if (!cn.call_prelogin())
    {
        printf("PRELOGIN - FAILED\n");
        return 1;
    }
    printf("PRELOGIN - OK\n");
#endif

    if (!cn.call_login7(argv[1], argv[3], argv[4], argv[5]))
    {
        printf("AUTH - FAILED\n");
        return 1;
    }

    printf("AUTH - OK\n");

    if (!cn.call_sql_batch(argv[6]))
    {
        printf("SQL - FAILED\n");
        return 1;
    }

    printf("SQL - OK\n");

    return 0;
}
