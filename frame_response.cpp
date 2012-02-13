#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS // for PRI* macros
#endif
#include <inttypes.h>

#include "debug.h"
#include "frame_response.h"

namespace tds
{

frame_response::frame_response()
	: auth_success(false)
{
}

bool frame_response::decode(buffer& input)
{
	while (!input.empty())
	{
		frame_token_header hdr;

		if (!input.fetch(hdr.type))
			return false;
		TP_DEBUG("processing token: 0x%02x", hdr.type);

		int token_class = frame_token_get_class(hdr.type);

		switch (token_class)
		{
		case tce_zero_len:
			TP_DEBUG("token is zero length"); break;
		case tce_fixed_len:
			TP_DEBUG("token is fixed length"); break;
		case tce_var_len:
			TP_DEBUG("token is variable length");
			if (!input.fetch(hdr.length))
				return false;
			break;
		case tce_var_count:
			// COLMETADATA & ALTMETADATA
			TP_DEBUG("token is variable count"); break;
		default:
			TP_DEBUG("unknown token class %d", token_class);
		}

		switch(hdr.type)
		{
		case ft_error:
		{
			frame_token_error e;
			if (!e.decode(input))
				return false;
			errors.push_back(e);
			break;
		}
		case ft_loginack:
		{
			frame_token_loginack ack;
			if (!ack.decode(input))
				return false;
			auth_success = true;
			break;
		}
		case ft_envchange:
		{
			frame_token_envchange elm;
			if (!elm.decode(hdr,input))
				return false;
			break;
		}
		case ft_done:
		{
			frame_token_done elm;
			if (!elm.decode(input))
				return false;
			break;
		}
		case ft_colmetadata:
		{
			columns_info.clear();
			rows.clear();
			if (!columns_info.decode(input))
				return false;
			break;
		}
		case ft_row:
		case ft_nbcrow:
		{
			TP_DEBUG("fetch row");
			frame_token_row row(hdr.type);
			if (!row.decode(columns_info, input))
				return false;

			rows.push_back(row);
			break;
		}
		default:
		{
			size_t size = input.size();
			TP_DEBUG("size=%" PRIiPTR, size);
			TP_DEBUG("hdr.length=%d", hdr.length);
			std::string unknown;
			unknown.resize(size);
			input.copy_to(0, size, &unknown[0]);
			input.drain(size);
			TP_DEBUG_DUMP(&unknown[0], unknown.size(), "error: unknown token: 0x%02x (len %d)", hdr.type, (int)size);
		}
		};
	}

	return true;
}

} // namespace tds
