#include "tdslite/tds_frames.h"
#include "tdslite/debug.h"

#include <gtest/gtest.h>

TEST(Frame, ResponseIntRow)
{
#define assert(v) EXPECT_TRUE(v)

    /* this is server response to: select count(*) from [table]r
     * response should contains:
     * 1 row: count(*) = 0
     */
    const unsigned char sample[] = {
        /* header */
        0x04,
        0x01,0x00,0x27,0x00,0x37,0x01,0x00,
        /* table response */
        0x81, // token: ft_colmetadata, tce_var_count (0x00)
        0x01,0x00, // number of items (1)
        0x00,0x00,0x00,0x00, // UserType
        0x01,0x00, // flags
        0x26, // VARLENTYPE: INTNTYPE
        0x04, // INTN length (1 by
        0x00, // ColName: ""
        0xd1, // token: ft_row, tce_zero_len
        0x04, // varbyte_len: 4 bytes
        0x00,0x00,0x00,0x00, // value = 0
        0xfd, // token: ft_done, tce_fixed_len
        0x10,0x00, // Status
        0xc1,0x00, // CurCmd
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00 // DoneRowCount
        };

    unsigned char encoded[sizeof(sample)];

    tds::buffer tmp;
    tmp.put(sample, sizeof(sample));
    tds::frame_header header;
    tds::frame_response body;

    tmp.fetch(header);
    assert(true == body.decode(tmp));
    assert(1 == body.rows.size());
    assert(1 == body.rows[0].data.size());
    assert(false == body.rows[0].data[0].isNull);
    assert(0 == body.rows[0].data[0].data.v_bigint);
}
