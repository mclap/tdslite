#include "tdslite/tds_frames.h"
#include "tdslite/debug.h"

#include <gtest/gtest.h>

#include <iostream>

#define RP(row,column) (body.rows[row].data[column])
#define ISNULL(row,column) (RP(row,column).isNull)
#define VINT(row,column) (RP(row,column).data.v_bigint)

TEST(Frame, ResponseNbcRow)
{
    // test 1: * SELECT NULL AS c1, NULL AS c2
    // result: 1 row (NULL, NULL)
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
        EXPECT_TRUE(body.decode(tmp));
        EXPECT_EQ(1, body.rows.size());

        EXPECT_EQ(2, body.rows[0].data.size());
        EXPECT_EQ(true, ISNULL(0,0));
        EXPECT_EQ(true, ISNULL(0,1));
        EXPECT_EQ(0, VINT(0,0));
        EXPECT_EQ(0, VINT(0,1));
    }

    // test 2: SELECT NULL AS c1, NULL AS c2 UNION SELECT 1,NULL UNION SELECT NULL,2
    // result: (NULL, NULL), (1,NULL), (NULL,2)
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
        EXPECT_TRUE(body.decode(tmp));
        EXPECT_EQ(3, body.rows.size());

        // row 0: NULL, NULL
        EXPECT_EQ(2, body.rows[0].data.size());
        EXPECT_EQ(true, ISNULL(0,0));
        EXPECT_EQ(true, ISNULL(0,1));
        EXPECT_EQ(0, VINT(0,0));
        EXPECT_EQ(0, VINT(0,1));

        // row 1: NULL, 2
        EXPECT_EQ(2, body.rows[1].data.size());
        EXPECT_EQ(true, ISNULL(1,0));
        EXPECT_EQ(false, ISNULL(1,1));
        EXPECT_EQ(0, VINT(1,0));
        EXPECT_EQ(2, VINT(1,1));

        // row 1: NULL, 2
        EXPECT_EQ(2, body.rows[2].data.size());
        EXPECT_EQ(false, ISNULL(2,0));
        EXPECT_EQ(true, ISNULL(2,1));
        EXPECT_EQ(1, VINT(2,0));
        EXPECT_EQ(0, VINT(2,1));
    }
}
