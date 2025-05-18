#pragma once

namespace tds
{

/* ...  NBCROW and ROW tokens can be intermixed
 * in the same result set.
*/

struct frame_token_row
{
    frame_token_row(const uint8_t _type)
        : type(_type)
    { }

    //std::deque<column_data_ptr> columns;
    std::deque<column_data> data;
    const uint8_t type;

    bool decode(const frame_token_colmetadata& meta, buffer& input);
    const bool isNBC() {
        return type == ft_nbcrow;
    }
};

#if 0
struct frame_token_nbcrow : public frame_token_row
{
    frame_token_nbcrow() { }
    virtual ~frame_token_nbcrow() { }

    virtual bool decode(const frame_token_colmetadata& meta, buffer& input);
    /*
     * type
     * text_pointer b_varbyte
     * timestamp (8byte)
     * data TYPE_VARBYTE
     * NullBitmap (count(columns)+7)/8 bytes
     * ColumnData [TestPointer Timestamp] Data
     * AllColumnData *ColumnData
     */
};
#endif

} // namespace tds
