#include <arpa/inet.h>
#include <inttypes.h>

#include "tds_frames.h"

#include "debug.h"

namespace tds
{

void tds_encrypt(void *ptr, size_t len)
{
	unsigned char *p = (unsigned char *)ptr;
	while (len > 0)
	{
		unsigned char b = *p;
		*p++ = ( ((b & 0xF0) >> 4) | ((b & 0x0F) << 4) ) ^ 0xA5;
		len--;
	}
}

void tds_decrypt(void *ptr, size_t len)
{
	unsigned char *p = (unsigned char *)ptr;
	while (len > 0)
	{
		unsigned char b = *p & 0xA5;
		*p++ = ( ((b & 0xF0) >> 4) | ((b & 0x0F) << 4) );
		len--;
	}
}
iconv_convert::iconv_convert(const char *_from, const char *_to)
	: from(_from), to(_to), h(0)
{
}

iconv_convert::~iconv_convert()
{
	if (!h)
		return;

	iconv_close(h);
	h = 0;
}

void iconv_convert::convert(const void *ptr, const size_t len, std::vector<char>& buf)
{
	if (!h)
		h = iconv_open(to, from);

	buf.resize(len*16);

	const char *inptr = (const char *)ptr;
	char *outptr = &buf[0];
	size_t inleft = len;
	size_t outleft = buf.size();

	int nconv = iconv(h, &inptr, &inleft, &outptr, &outleft);
	TP_DEBUG("nconv=%d, errno=%d, outleft=%d, len=%d", nconv, errno, (int)outleft, (int)len);

	buf.resize(buf.size() - outleft);
	TP_DEBUG_DUMP(&buf[0], buf.size(), "converted data:");
}

buffer::buffer()
	: to_utf16("UTF-8", "UCS-2LE")
	, to_utf8("UCS-2LE", "UTF-8")
{
}

buffer::~buffer()
{
}

bool buffer::copy_to(size_t off, size_t len, void *dst)
{
	TP_DEBUG("copy %d bytes", (int)len);
	memcpy(dst, &data[off], len);
	return true;
}

bool buffer::copy_to_utf8(size_t off, size_t len, std::string& dst, byte_filter f)
{
	std::vector<char> tmp, buf;
	tmp.assign(data.begin()+off, data.begin()+off+len*2);

	if (f)
		f(&tmp[0], len*2);

	to_utf8.convert(&tmp[0], len * 2, buf);

	dst.assign(&buf[0], buf.size());

	return true;
}

bool buffer::empty()
{
	return data.empty();
}

size_t buffer::size()
{
	return data.size();
}

void buffer::clear()
{
	data.clear();
}

void buffer::drain(size_t size)
{
	TP_DEBUG("drain %d bytes of %d", (int)size, (int)data.size());
	data.erase(data.begin(), data.begin()+size);
}

void buffer::put(const void *ptr, size_t size)
{
	const unsigned char *begin = reinterpret_cast<const unsigned char *>(ptr),
	      *end = begin+size;

	std::copy (begin, end, std::back_inserter(data));
}

void buffer::put(const std::string& value, bool include_nul)
{
	put(&value[0], value.size() + (include_nul ? 1 : 0));
}

void buffer::put_utf16(const std::string& value, buffer::byte_filter f)
{
	std::vector<char> buf;
	to_utf16.convert(value.c_str(), value.size(), buf);
	if (f)
		f(&buf[0], buf.size());

	put(&buf[0], buf.size());
}

void buffer::put(const buffer& value)
{
	put(&value.data[0], value.data.size());
}

bool buffer::push(net_t cn)
{
	int rc = net_send(cn, &data[0], data.size());
	if (0 == rc)
		return true;

	perror("net_send");
	return false;
}

bool buffer::pull(net_t cn, size_t size)
{
	data.resize(size);
	int rc = net_read(cn, &data[0], size);
	if (size == rc)
		return true;

	perror("net_read");
	return false;
}

bool buffer::fetch_b_varchar(std::string& out)
{
	uint8_t len;
	fetch(len);
	copy_to_utf8(0, len, out);
	drain(len*2);
	return true;
}

bool buffer::fetch_us_varchar(std::string& out)
{
	uint16_t len;
	fetch(len);
	copy_to_utf8(0, len, out);
	drain(len*2);
	return true;
}

void buffer::put_b_varchar(const std::string& value)
{
	std::vector<char> buf;
	to_utf16.convert(value.c_str(), value.size(), buf);
	size_t len = buf.size() >> 1;

	if (len > 255)
	{
		put((uint8_t)0);
		return;
	}

	put((uint8_t)len);
	put(&buf[0], buf.size());
}

void buffer::put_us_varchar(const std::string& value)
{
	std::vector<char> buf;
	to_utf16.convert(value.c_str(), value.size(), buf);
	size_t len = buf.size() >> 1;

	if (len > 65535)
	{
		put((uint16_t)0);
		return;
	}

	put((uint16_t)len);
	put(&buf[0], buf.size());
}

frame_header::frame_header()
	: type(type_unknown)
	, status(eom)
	, ns_length(0)
	, ns_spid(0)
	, packet_id(0)
	, window(0)
{
}

bool frame_header::push(net_t cn)
{
	int rc = net_send(cn, this, sizeof(*this));
	if (0 == rc)
		return true;

	perror("net_send");
	return false;
}

bool frame_header::pull(net_t cn)
{
	int rc = net_read(cn, this, sizeof(*this));
	if (sizeof(*this) == rc)
		return true;

	perror("net_read");
	return false;
}

void frame_prelogin::option::encode(buffer& output, const unsigned short base_offset)
{
	output.put(token);
	output.put(htons(offset + base_offset));
	output.put(htons(length));
}

bool frame_prelogin::encode_option(buffer& output, const option::token_type token, const void *data, const short size)
{
	options.push_back(option(token, output.size(), size));
	output.put(data, size);
	return true;
}

bool frame_prelogin::encode(buffer& output)
{
#if 0
        static const unsigned char prelogin[] = {
                0x00, 0x00, 0x15, 0x00, 0x06, 0x01, 0x00, 0x1b,
                0x00, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x01, 0x03,
                0x00, 0x1d, 0x00, 0x00, 0xff, 0x08, 0x00, 0x01,
                0x55, 0x00, 0x00, 0x02, 0x00
        };

        output.put(prelogin, sizeof(prelogin));
	return true;

#else
	options.clear();
	options_buffer.clear();

	version.version = TDS_VERSION_2008_B;
	version.sub_build = 0;
	encryption = 0;
	mars = 0;
	instopt = 0;
	thread_id = 0x424242;

	/* options */
#define _ENC(opt) encode_option(options_buffer, option::tt_##opt, &opt, sizeof(opt))
	_ENC(version);
	_ENC(encryption);
	_ENC(instopt);
	_ENC(thread_id);
	_ENC(mars);
#undef _ENC

	/* update options */
	unsigned short options_offset = sizeof(option) * options.size() + 1 /* tt_terminator */;
	std::vector<option>::iterator iter = options.begin();
	for ( ; iter != options.end(); ++iter)
	{
		iter->encode(output, options_offset);
	}

	output.put((unsigned char)option::tt_terminator);
	output.put(options_buffer);

	return true;
#endif
}

bool frame_prelogin::decode(buffer& input)
{
	size_t offset = 0;
	while (offset < input.size())
	{
		option hdr;
		input.copy_to(offset, 1, &hdr.token);
		if (hdr.token == 0xff)
			break;

		input.copy_to(offset, sizeof(hdr), &hdr);
		offset += sizeof(hdr);

		hdr.offset = ntohs(hdr.offset);
		hdr.length = ntohs(hdr.length);

		TP_DEBUG("processing option: 0x%02x", hdr.token);
		switch(hdr.token)
		{
		case option::tt_version:
		{
			option_version ver;
			input.copy_to(hdr.offset, hdr.length, &ver);
			TP_DEBUG("version=%08x, sub_build=%d", ver.version,
				ver.sub_build);
			break;
		}
		default:
		{
			std::string raw;
			raw.resize(hdr.length);
			input.copy_to(hdr.offset, hdr.length, &raw[0]);

			TP_DEBUG_DUMP(&raw[0], raw.size(), "option: token=%d, len=%d", (int)hdr.token, (int)hdr.length);
		}
		}
	}

	input.drain(input.size());
	return true;

	return true;
}

frame_login7::frame_login7()
{
	memset(&fixed, 0, sizeof(fixed));

	fixed.tds_version = htonl(TDS_VERSION_2008_B);
	fixed.packet_size = 65536;
	fixed.client_pid = 1; // FIXME
	fixed.flags2.f_odbc = 1;
	fixed.flags2.f_language = 1;
}

bool frame_login7::encode_ref(const std::string& data, ref_type& ref, buffer& ref_data, buffer::byte_filter f)
{
	ref.off = sizeof(fixed) + ref_data.size();
	ref.len = 0;

	if (data.empty())
		return true;

	ref.len = data.size();

	ref_data.put_utf16(data, f);
	return true;
}

bool frame_login7::encode(buffer& output)
{
	buffer ref_data;
#define EREF(v,f) do { \
	TP_DEBUG("encode %s (%d): %s", #v, (int)v.size(), v.c_str()); \
	encode_ref(v, fixed.v, ref_data, f); \
} while(0)
	EREF(client_host,0);
	EREF(user_name,0);
	EREF(user_pass,tds_encrypt);
	EREF(app_name,0);
	EREF(server_name,0);
	EREF(interface_name,0);
	EREF(language,0);
	EREF(database,0);
	EREF(sspi,0);
	EREF(db_file,0);
	EREF(change_password,0);
#undef EREF

	/* TODO: Fix password */

	fixed.length = sizeof(fixed) + ref_data.size();

	output.put(fixed);
	output.put(ref_data);

	return true;
	}

bool frame_login7::decode_ref(buffer& input, ref_type& ref, std::string& value, buffer::byte_filter f)
{
	if (ref.len > 0)
		input.copy_to_utf8(ref.off, ref.len, value, f);
	return true;
}

bool frame_login7::decode(buffer& input)
{
	TP_DEBUG("fetch fixed");
	input.copy_to(0, sizeof(fixed), &fixed);
	TP_DEBUG("byte order: %d", fixed.flags1.f_byte_order);
	TP_DEBUG("length: %d", fixed.length);

#define DREF(v,f) do {\
	TP_DEBUG("fetch %s (off=%04x, len=%04x)", #v, fixed.v.off, fixed.v.len); \
	decode_ref(input, fixed.v, v, f); \
	TP_DEBUG("%s: %s", #v, v.c_str()); \
} while(0)
	DREF(client_host,0);
	DREF(user_name,0);
	DREF(user_pass,tds_decrypt);
	DREF(app_name,0);
	DREF(server_name,0);
	DREF(interface_name,0);
	DREF(language,0);
	DREF(database,0);
	//input.copy_to(ref.off, ref.len, &fixed.client_id);
	DREF(sspi,0);
	DREF(db_file,0);
	DREF(change_password,0);
#undef DREF

	return true;
}

bool frame_token_error::encode(buffer& output)
{
	output.put(&fixed, sizeof(fixed));

	output.put_us_varchar(error_text);
	output.put_b_varchar(server_name);
	output.put_b_varchar(proc_name);
	output.put(line);

	return true;
}

bool frame_token_error::decode(buffer& input)
{
	input.copy_to(0, sizeof(fixed), &fixed);
	input.drain(sizeof(fixed));
	input.fetch_us_varchar(error_text);
	input.fetch_b_varchar(server_name);
	input.fetch_b_varchar(proc_name);
	input.fetch(line);

	TP_DEBUG("bytes left: %d", (int)input.size());
	return true;
}

bool frame_token_loginack::decode(buffer& input)
{
	input.copy_to(0, sizeof(fixed1), &fixed1);
	input.drain(sizeof(fixed1));
	input.fetch_b_varchar(prog_name);
	input.copy_to(0, sizeof(fixed2), &fixed2);
	input.drain(sizeof(fixed2));

	TP_DEBUG("bytes left: %d", (int)input.size());
	TP_DEBUG("prog_name: %s (%d.%d %d.%d)", prog_name.c_str(),
			fixed2.ver_major,
			fixed2.ver_minor,
			fixed2.build_num_hi,
			fixed2.build_num_lo);
	TP_DEBUG("interface: %d, version %08x",
			fixed1.interface,
			fixed1.tds_version);
	return true;
}

bool frame_token_envchange::decode(const frame_token_header& hdr, buffer& input)
{
	input.fetch(type);
	switch(type)
	{
	case 4: // packet size
		input.fetch_b_varchar(s_old);
		input.fetch_b_varchar(s_new);
		TP_DEBUG("envchange (pkt size): %s (old: %s)", s_new.c_str(), s_old.c_str());
		break;
	default:
		TP_DEBUG("envchange type: %d (ignore)", type);
		input.drain(hdr.length-1);
	}

	return true;
}

bool column_info::decode(buffer& input)
{
	if (!input.fetch(&fixed1, sizeof(fixed1)))
		return false;

	TP_DEBUG("user_type=%08x", fixed1.user_type);
	TP_DEBUG("flags=%04x", fixed1.flags);

	if (!input.fetch(type))
		return false;

	TP_DEBUG("type=%02x", type);

	memset(&length, 0, sizeof(length));
	switch(type)
	{
	case dt_intn:
	{
		uint8_t _len;
		if (!input.fetch(_len))
			return false;
		length = _len;
		TP_DEBUG("INTN(%d)", (int)length);
		break;
	}
	default:
	{
		size_t size = input.size();
		std::string unknown;
		unknown.resize(size);
		input.copy_to(0, size, &unknown[0]);
		input.drain(size);
		TP_DEBUG_DUMP(&unknown[0], unknown.size(), "error: unknown column");
		return false;
	}
	}

	if (!input.fetch_b_varchar(col_name))
		return false;

	TP_DEBUG("col_name: [%s]", col_name.c_str());

	return true;
}

void frame_token_colmetadata::clear()
{
	info.clear();
}

bool frame_token_colmetadata::decode(buffer& input)
{
	unsigned short count;
	if (!input.fetch(count))
		return false;

	TP_DEBUG("columns: %u", count);
	if (count < 1 || count == 0xFFFF)
		return true;

	unsigned short i;
	for (i = 0; i < count; ++i)
	{
		column_info elm;
		if (!elm.decode(input))
			return false;

		info.push_back(elm);
	}
	TP_DEBUG("COLMETADATA is parsed. columns: %d", (int)info.size());

	return true;
}

bool column_data::decode(const column_info& info, buffer& input)
{
	TP_DEBUG("info.type=0x%02x, info.length=%d", info.type, info.length);

	switch(info.type)
	{
	case dt_intn:
		TP_DEBUG("fetch INTN(%d)", (int)info.length);
		switch(info.length)
		{
		case 1:
		{
			int8_t v;
			if (!input.fetch(v))
				return false;
			data.v_bigint = v;
			return true;
		}	
		case 2:
		{
			int16_t v;
			if (!input.fetch(v))
				return false;
			data.v_bigint = v;
			return true;
		}
		case 4:
		{
			int32_t v;
			if (!input.fetch(v))
				return false;
			data.v_bigint = v;
			TP_DEBUG("v_bigint=%d", v);
			return true;
		}
		case 8:
		{
			int64_t v;
			if (!input.fetch(v))
				return false;
			data.v_bigint = v;
			return true;
		}
		}
	default:
		return false;
	}

	return true;
}

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
			/* TODO: fixme! May have many "DONE" tokens */
			input.drain(input.size());
			break;
		case ft_colmetadata:
		{
			columns_info.clear();
			rows.clear();
			if (!columns_info.decode(input))
				return false;
			break;
		}
		case ft_row:
		{
			TP_DEBUG("fetch row");
			frame_token_row row;
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

bool query_header::encode(buffer& output)
{
	buffer data;
	if (!encode_data(data))
		return false;

	fixed.length = data.size() + sizeof(fixed);
	output.put(&fixed, sizeof(fixed));
	output.put(data);

	return true;
}

bool query_header_trans::encode_data(buffer& output)
{
#define _X(f) TP_DEBUG_DUMP(&f, sizeof(f), #f)
	_X(fixed.transaction_descriptor);
	_X(fixed.outstanding_request_count);
#undef _X
	output.put(&fixed, sizeof(fixed));
	return true;
}

query_headers::query_headers()
{
}

query_headers::~query_headers()
{
	clear();
}

void query_headers::clear()
{
	value_type::iterator iter = headers.begin();
	for ( ; iter != headers.end(); ++iter)
		if (*iter)
			delete *iter;

	headers.clear();
}

bool query_headers::encode(buffer& output)
{
	buffer body;

	value_type::iterator iter = headers.begin();
	for ( ; iter != headers.end(); ++iter)
		if (*iter && !(*iter)->encode(body))
			return false;

	total_len = body.size() + sizeof(total_len);
	output.put(total_len);
	output.put(body);

	return true;
}

bool frame_sql_batch::encode(buffer& output)
{
	if (!auto_commit)
	{
		TP_DEBUG("only auto commit mode is supported!");
		return false;
	}

	query_headers qh;
	qh.headers.push_back(new query_header_trans(1, 0));
	if (!qh.encode(output))
		return false;

	output.put_utf16(query);
	return true;
}

} // namespace tds
