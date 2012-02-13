#include "tds_frames.h"
#include "frame_token_row.h"

namespace tds
{

bool frame_token_row::decode(const frame_token_colmetadata& meta, buffer& input)
{
	frame_token_colmetadata::info_type::const_iterator iter =
		meta.info.begin();
	for ( ; iter != meta.info.end(); ++ iter)
	{
		column_data elm;
		if (!elm.decode(*iter, input))
			return false;

		data.push_back(elm);
	}

	return true;
}

} // namespace tds
