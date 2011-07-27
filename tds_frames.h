#pragma once
#include <vector>
#include <string>
#include <deque>

#include <iconv.h>
#include "net.h"

#define TDS_VERSION_2005	0x02000972
#define TDS_VERSION_2008_A	0x03000A73
#define TDS_VERSION_2008_B	0x03000B73
#define TDS_VERSION_GEN_7_3	0x00000073
#define TDS_VERSION_GEN_7_2	0x00000072
#define TDS_VERSION_GEN_7_0	0x00000070

namespace tds
{

class iconv_convert
{
public:
	iconv_convert(const char *_from, const char *_to);

	~iconv_convert();

	void convert(const void *ptr, const size_t len, std::vector<char>& buf);

private:
	iconv_t h;
	const char *from, *to;
};

class buffer
{
public:
	iconv_convert to_utf16;
	iconv_convert to_utf8;

	typedef void (*byte_filter)(void *ptr, size_t len);

	buffer();
	~buffer();

	template<typename T>
	bool fetch(T& value)
	{
		size_t len = sizeof(value);

		copy_to(0, len, &value);
		drain(len);
		return true;
	}

	bool copy_to(size_t off, size_t len, void *dst);

	bool copy_to_utf8(size_t off, size_t len, std::string& dst, byte_filter f = 0);

	bool empty();

	size_t size();

	void clear();
	void drain(size_t size);

	template<typename T>
	void put(const T& value)
	{
		const unsigned char *begin = reinterpret_cast<const unsigned char *>(&value),
		      *end = begin+sizeof(value);

		std::copy (begin, end, std::back_inserter(data));
	}

	void put(const void *ptr, size_t size);
	void put(const std::string& value, bool include_nul = false);
	void put_utf16(const std::string& value, buffer::byte_filter f = 0);
	void put(const buffer& value);
	bool push(net_t cn);
	bool pull(net_t cn, size_t size);

	bool fetch_b_varchar(std::string& out);
	bool fetch_us_varchar(std::string& out);
	void put_b_varchar(const std::string& value);
	void put_us_varchar(const std::string& value);

private:
	typedef std::vector<unsigned char> buffer_type;

	buffer_type data;
};

#pragma pack(push, 1)
struct frame_header
{
	enum type_e
	{
		type_unknown = 0,
		sql_batch = 1,
		rpc = 3,
		table_response = 4,
		tds7_login = 16,
		pre_login = 18,
	};

	enum status_flags
	{
		normal = 0x00,
		eom = 0x01,
		ignore = 0x02,
		reset_connection = 0x08,
		reset_connection_skiptran = 0x10,
	};

	unsigned char type;
	unsigned char status;
	uint16_t ns_length;
	uint16_t ns_spid;
	unsigned char packet_id;
	unsigned char window;

	frame_header();

	bool push(net_t cn);
	bool pull(net_t cn);
};

#pragma pack(pop)

struct frame_prelogin 
{
	struct option
	{
		unsigned char token;
		std::string data;
	};

	uint32_t version;
	uint16_t sub_build;
	unsigned char encryption;

	uint32_t thread_id;
	std::string instance_name;

	std::vector<option> options;

	bool encode(buffer& output);
	bool decode(const buffer& input);
};

struct frame_login7
{
#pragma pack(push, 1)
	struct ref_type
	{
		uint16_t off;
		uint16_t len;
	};

	struct fixed_type
	{
		uint32_t length;
		uint32_t tds_version; /* 0x71 for 7.1 */
		uint32_t packet_size; // = 65536;
		uint32_t client_prog_ver;
		uint32_t client_pid;
		uint32_t connection_id;

		/* offset: 24, 0x18 */

		struct {
			unsigned char f_byte_order:1;
			unsigned char f_char:1;
			unsigned char f_float:2;
			unsigned char f_dump_load:1;
			unsigned char f_use_db:1;
			unsigned char f_database:1;
			unsigned char f_set_lang:1;
		} flags1;

		/* offset: 25, 0x19 */

		struct {
			unsigned char f_language:1;
			unsigned char f_odbc:1;
			unsigned char f_reserved:2;
			unsigned char f_user_type:3;
			unsigned char f_int_security:1;
		} flags2;

		/* offset: 26, 0x1a */

		struct {
			unsigned char f_sql_type:4;
			unsigned char f_oledb:1;
			unsigned char f_reserved:3;
		} type_flags;

		/* offset: 27, 0x1b */

		struct {
			unsigned char f_change_password:1;
			unsigned char f_user_instance:1;
			unsigned char f_send_yukon_binaryxml:1;
			unsigned char f_unknown_collation_handling:1;
			unsigned char f_reserved:4;
		} flags3;

		/* offset: 28, 0x1c */

		uint32_t client_time_zone;

		/* offset: 32, 0x20 */

		struct {
			uint32_t lcid:12;
			struct {
				unsigned char f_ignore_case:1;
				unsigned char f_ignore_accent:1;
				unsigned char f_ignore_width:1;
				unsigned char f_ignore_kana:1;
				unsigned char f_binary:1;
				unsigned char f_reserved:3;
			} col_flags;
			unsigned char version:4;
		} client_lcid;

		/* offset: 36, 0x24 */

		ref_type client_host
		, user_name

		/* offset: 44, 0x2c */

		, user_pass

		/* offset: 48, 0x30 */

		, app_name
		, server_name
		, unused
		, interface_name
		, language
		, database
		;

		unsigned char client_id[6];

		ref_type sspi
			, db_file
			, change_password
			;
		uint32_t sspi_long;
	} fixed;
#pragma pack(pop)

	std::string client_host;
	std::string user_name;
	std::string user_pass;
	std::string app_name;
	std::string server_name;
	std::string interface_name;
	std::string language;
	std::string database;
	std::string sspi;
	std::string db_file;
	std::string change_password;

	frame_login7();

	bool encode_ref(const std::string& data, ref_type& ref, buffer& ref_data, buffer::byte_filter f = 0);
	bool encode(buffer& output);
	bool decode_ref(buffer& input, ref_type& ref, std::string& value, buffer::byte_filter f = 0);
	bool decode(buffer& input);
};


enum frame_token_e
{
	ft_error = 0xaa,
	ft_loginack = 0xad,
	ft_envchange = 0xe3,
	ft_done = 0xfd
};

#pragma pack(push, 1)
struct frame_token_header
{
	uint8_t type;
	uint16_t length;
};
#pragma pack(pop)

struct frame_token_error
{
#pragma pack(push, 1)
	struct
	{
		uint32_t error_number;
		uint8_t error_state;
		uint8_t error_class;
	} fixed;
#pragma pack(pop)

	std::string error_text;
	std::string server_name;
	std::string proc_name;
	uint32_t line;

	bool encode(buffer& output);
	bool decode(buffer& input);
		//error_text; US_VAR: 
		//server_name; B_VAR
		//proc_name ; BVAR
};

struct frame_token_loginack
{
#pragma pack(push, 1)
	struct
	{
		uint8_t interface;
		uint32_t tds_version;
	} fixed1;
#pragma pack(pop)

	std::string prog_name;

#pragma pack(push, 1)
	struct
	{
		uint8_t ver_major;
		uint8_t ver_minor;
		uint8_t build_num_hi;
		uint8_t build_num_lo;
	} fixed2;
#pragma pack(pop)

	bool decode(buffer& input);
};

struct frame_token_envchange
{
	uint8_t type;
	std::string s_old, s_new;

	bool decode(const frame_token_header& hdr, buffer& input);
};

struct frame_response
{
	std::deque<frame_token_error> errors;

	bool decode(buffer& input);
};

} // namespace tds
