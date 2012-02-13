#pragma once

namespace tds
{

struct frame_token_row
{
	frame_token_row() { }
	virtual ~frame_token_row() { }
	//std::deque<column_data_ptr> columns;
	std::deque<column_data> data;

	virtual bool decode(const frame_token_colmetadata& meta, buffer& input);
};

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

} // namespace tds
