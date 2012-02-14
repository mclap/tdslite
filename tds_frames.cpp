#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS // for PRI* macros
#endif
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <inttypes.h>

#include "tds_frames.h"

#include "debug.h"

#if defined(linux)
#define ICONV_CHAR_PP(v) (char **)(v)
#else
#define ICONV_CHAR_PP(v) (v)
#endif

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

	int nconv = iconv(h, ICONV_CHAR_PP(&inptr), &inleft, &outptr, &outleft);
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

bool buffer::copy_dos_to_utf8(size_t off, size_t len, std::string& dst, byte_filter f)
{
	std::vector<char> tmp, buf;
	tmp.assign(data.begin()+off, data.begin()+off+len);

	if (f)
		f(&tmp[0], len);

	// FIXME: dos_to_utf8.convert(&tmp[0], len, buf);
	buf = tmp;

	dst.assign(&buf[0], buf.size());
	TP_DEBUG("dos: \"%s\"", dst.c_str());

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
	case dt_bit:
	case dt_int1:
		length = 1;
		break;
	case dt_int2:
		length = 2;
		break;
	case dt_int4:
		length = 4;
		break;
	case dt_int8:
		length = 8;
		break;
	case dt_bigvarchar:
	case dt_nvarchar:
	{
		uint16_t _len;
		if (!input.fetch(_len))
			return false;
		length = _len;
		switch (type) {
		case dt_bigvarchar: TP_DEBUG("BIGVARCHAR(%d)", (int)length); break;
		case dt_nvarchar: TP_DEBUG("NVARCHAR(%d)", (int)length); break;
		}

		if (!input.fetch(&collation, sizeof(collation)))
			return false;
		break;
	}
	case dt_bigbinary:
	{
		uint16_t _len;
		if (!input.fetch(_len))
			return false;
		length = _len;
		switch (type) {
		case dt_bigbinary: TP_DEBUG("BIGBINARY(%d)", (int)length); break;
		}

		break;
	}
	default:
	{
		size_t size = input.size();
		std::string unknown;
		unknown.resize(size);
		input.copy_to(0, size, &unknown[0]);
		input.drain(size);
		TP_DEBUG_DUMP(&unknown[0], unknown.size(), "error: unknown column: 0x%02x", type);
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

column_data::column_data()
	: isNull(false)
{
	memset(&data, 0, sizeof(data));
}

bool column_data::decode(const column_info& info, buffer& input)
{
	TP_DEBUG("info.type=0x%02x, info.length=%d", info.type, info.length);

	// FIXME: parse
	// [TextPointer TimeStamp]

	switch(info.type)
	{
	case dt_intn:
		uint8_t varbyte_len;
		if (!input.fetch(varbyte_len))
			return false;

		TP_DEBUG("varbyte_len=%d", varbyte_len);
		if (varbyte_len == 0)
		{
			isNull = true;
			return true;
		}

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
			TP_DEBUG("v_bigint=%" PRId64, v);
			return true;
		}
		}
		break;
	case dt_bigvarchar:
		uint16_t len;
		if (!input.fetch(len))
			return false;

		TP_DEBUG("len=%d", len);
		if (len == 0)
		{
			isNull = true;
			return true;
		}

		if (!input.copy_dos_to_utf8(0, len, raw))
			return false;

		input.drain(len);

		break;
	default:
		return false;
	}

	return true;
}

bool frame_token_done::decode(buffer& input)
{
	if (!input.fetch(&fixed, sizeof(fixed)))
		return false;

	TP_DEBUG("done.status=%04x", fixed.status);
	TP_DEBUG("done.curcmd=%04x", fixed.curcmd);
	TP_DEBUG("done.rowcount=%" PRIu64, fixed.rowcount);
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
