#include "tdslite/tds_frames.h"
#include "tdslite/debug.h"

#include <gtest/gtest.h>

#include <iostream>

#define RP(row,column) (body.rows[row].data[column])
#define ISNULL(row,column) (RP(row,column).isNull)
#define VINT(row,column) (RP(row,column).data.v_bigint)

TEST(Frame, ResponseNbcRow)
{
#define assert(v) EXPECT_TRUE(v)

    /*
     * test 1: * SELECT NULL AS c1, NULL AS c2
     * result: 1 row (NULL, NULL)
     */

    {
        const unsigned char sample[] = {
            0x04,0x01,0x00,0x34,0x00,0x3b,0x01,0x00,0x81,0x02,
            0x00,0x00,0x00,0x00,0x00,0x21,0x00,0x26,0x04,0x02,
            0x63,0x00,0x31,0x00,0x00,0x00,0x00,0x00,0x21,0x00,
            0x26,0x04,0x02,0x63,0x00,0x32,0x00,0xd2,0x03,0xfd,
            0x10,0x00,0xc1,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
            0x00,0x00 };

        tds::buffer tmp;
        tmp.put(sample, sizeof(sample));
        tds::frame_header header;
        tds::frame_response body;

        tmp.fetch(header);
        assert(true == body.decode(tmp));
        assert(1 == body.rows.size());

        assert(2 == body.rows[0].data.size());
        assert(true == ISNULL(0,0));
        assert(true == ISNULL(0,1));
        assert(0 == VINT(0,0));
        assert(0 == VINT(0,1));
    }

    /*
     * test 2: SELECT NULL AS c1, NULL AS c2 UNION SELECT 1,NULL UNION SELECT NULL,2
     * result: (NULL, NULL), (1,NULL), (NULL,2)
     */
    {
        const unsigned char sample[] = {
            0x04,0x01,0x00,0x42,0x00,0x3d,0x01,0x00,0x81,0x02,
            0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x26,0x04,0x02,
            0x63,0x00,0x31,0x00,0x00,0x00,0x00,0x00,0x01,0x00,
            0x26,0x04,0x02,0x63,0x00,0x32,0x00,0xd2,0x03,0xd1,
            0x00,0x04,0x02,0x00,0x00,0x00,0xd1,0x04,0x01,0x00,
            0x00,0x00,0x00,0xfd,0x10,0x00,0xc1,0x00,0x03,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00 };

        tds::buffer tmp;
        tmp.put(sample, sizeof(sample));
        tds::frame_header header;
        tds::frame_response body;

        tmp.fetch(header);
        assert(true == body.decode(tmp));
        assert(3 == body.rows.size());

        // row 0: NULL, NULL
        assert(2 == body.rows[0].data.size());
        assert(true == ISNULL(0,0));
        assert(true == ISNULL(0,1));
        assert(0 == VINT(0,0));
        assert(0 == VINT(0,1));

        // row 1: NULL, 2
        assert(2 == body.rows[1].data.size());
        assert(true == ISNULL(1,0));
        assert(false == ISNULL(1,1));
        assert(0 == VINT(1,0));
        assert(2 == VINT(1,1));

        // row 1: NULL, 2
        assert(2 == body.rows[2].data.size());
        assert(false == ISNULL(2,0));
        assert(true == ISNULL(2,1));
        assert(1 == VINT(2,0));
        assert(0 == VINT(2,1));
    }
}
