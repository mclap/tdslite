#include "tdslite/tds_frames.h"
#include "tdslite/debug.h"

#include <gtest/gtest.h>

TEST(Frame, Response)
{
    const unsigned char sample[] = {
        // header
        0x04, // type
        01, // status
        0x00, 0x33, // lenght
        0x00, 0x00, // SPID
        0x01, // packet id
        0x00, // window
        // table response
        0x81, // ft_colmetadata
        0x01, 0x00, // count
        0x00, 0x00, 0x00, 0x00, // user type
        0x20, 0x00, // flags
        0xA7, // type: dt_bigvarchar
        0x03, 0x00, // len
        0x09, 0x04, 0xD0, 0x00, 0x34, // collation
        0x03, // len
        0x62, 0x00, 0x61, 0x00, 0x72, 0x00, // column name "bar"
        // rows
        0xD1, // ft_row
        0x03, 0x00, // len
        0x66, 0x6F, 0x6F, // column[0] data: "foo"
        0xFD, // ft_done
        0x10, 0x00, // status
        0xC1, 0x00, // curcmd
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // rowcount
        };

    unsigned char encoded[sizeof(sample)];

    tds::buffer tmp;
    tmp.put(sample, sizeof(sample));
    tds::frame_header header;
    tds::frame_response body;

    tmp.fetch(header);
    EXPECT_TRUE(body.decode(tmp));
    EXPECT_EQ(1, body.rows.size());
    EXPECT_EQ(1, body.rows[0].data.size());
    EXPECT_EQ(false, body.rows[0].data[0].isNull);
    EXPECT_EQ("foo", body.rows[0].data[0].raw);
}
