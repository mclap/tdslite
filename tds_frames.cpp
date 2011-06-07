#include <arpa/inet.h>

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

size_t buffer::size()
{
	return data.size();
}

void buffer::clear()
{
	data.clear();
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

bool frame_prelogin::encode(buffer& output)
{
	output.put(version);
	output.put(sub_build);
	output.put(encryption);
	output.put(instance_name);
	output.put(thread_id);
	output.put((unsigned char)0x00); /* B_MARS */
	/* options */
	output.put((unsigned char)0xFF);

	return true;
}

bool frame_prelogin::decode(const buffer& input)
{
	return false;
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

} // namespace tds
