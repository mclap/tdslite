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

	bool fetch(void *ptr, size_t len)
	{
		copy_to(0, len, ptr);
		drain(len);
		return true;
	}

	template<typename T>
	bool fetch(T& value)
	{
		fetch(&value, sizeof(value));
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

/*
template <class Container>
class AutoDelete
{
public:
	typedef typename Container container_type;
	typedef container_type::iterator iterator;
	typedef container_type::const_iterator const_iterator;
	typedef container_type::value_type value_type;

	AutoDelete() { }

	~AutoDelete() { clear(); }

	void clear()
	{

	}
protected:
	Container
};
*/

#pragma pack(push, 1)
struct frame_header
{
	enum type_e
	{
		type_unknown = 0,
		sql_batch = 1,
		rpc = 3,
		table_response = 4,
		trans_request = 0x0E,
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
#pragma pack(push, 1)
	struct option
	{
		enum token_type
		{
			tt_version = 0x00,
			tt_encryption = 0x01,
			tt_instopt = 0x02,
			tt_thread_id = 0x03,
			tt_mars = 0x04,
			tt_terminator = 0xff
		};

		unsigned char token;
		unsigned short offset;
		unsigned short length;

		option()
		{
			token = 0;
			offset = length = 0;
		}

		option(const token_type _token, const unsigned short _offset, const unsigned short _length)
			: token(_token), offset(_offset), length(_length)
		{
		}

		void encode(buffer& output, const unsigned short base_offset);
	};

	struct option_version
	{
		uint32_t version;
		uint16_t sub_build;
	};
#pragma pack(pop)

	option_version version;
	unsigned char encryption;

	uint32_t thread_id;
	unsigned char mars;
	unsigned char instopt;

	std::vector<option> options;
	buffer options_buffer;

	bool encode(buffer& output);
	bool encode_option(buffer& output, const option::token_type token, const void *data, const short size);

	bool decode(buffer& input);
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

class query_header
{
public:
	enum types {
		query_notify = 0x0001,
		transaction_descriptor = 0x0002
	};

	query_header(const types& header_type)
	{
		fixed.length = 0;
		fixed.type = header_type;
	}

	virtual ~query_header() { }

	bool encode(buffer& output);

protected:
	virtual bool encode_data(buffer& output) = 0;

private:
#pragma pack(push, 1)
	struct {
		uint32_t length;
		uint16_t type;
	} fixed;
};

/*
 * from MSDN-TDS:
 * The TransactionDescriptor MUST be 0, and OutstandingRequestCount
 * MUST be 1 if the connection is operating in AutoCommit mode.
 * See [MSDN-Autocommit] for more information on autocommit
 * transactions.
 */
class query_header_trans : public query_header
{
public:
	query_header_trans(const uint32_t outstanding_request_count, const uint64_t transaction_descriptor)
		: query_header(query_header::transaction_descriptor)
	{
		fixed.outstanding_request_count = outstanding_request_count;
		fixed.transaction_descriptor = transaction_descriptor;
	}

	virtual ~query_header_trans() { }

protected:
	bool encode_data(buffer& output);

#pragma pack(push, 1)
	struct {
		uint64_t transaction_descriptor;
		uint32_t outstanding_request_count;
	} fixed;
#pragma pack(pop)
};

struct query_headers
{
	query_headers();
	~query_headers();

	uint32_t total_len;
	typedef std::vector<query_header *> value_type;

	value_type headers;

	bool encode(buffer& output);
	void clear();
};

struct frame_sql_batch
{
	frame_sql_batch()
		: auto_commit(true)
	{ }

	bool auto_commit;
	//all_headers
	//sqltext // unicode stream

	std::string query;

	bool encode(buffer& output);
};

enum frame_token_e
{
	ft_colmetadata = 0x81,
	ft_error = 0xaa,
	ft_loginack = 0xad,
	ft_row = 0xd1,
	ft_envchange = 0xe3,
	ft_done = 0xfd
};

enum frame_token_class_e
{
	tce_zero_len = 0x01,
	tce_fixed_len = 0x11,
	tce_var_len = 0x10,
	tce_var_count = 0x00,
};

inline int frame_token_get_class(unsigned char type)
{
	return (type & 0x30) >> 4;
}

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

/* type info

   FIXEDLENTYPE = NULLTYPE
              /
             INT1TYPE
            /
           BITTYPE
          /
         INT2TYPE
        /
       INT4TYPE
      /
     DATETIM4TYPE
    /
   FLT4TYPE
  /
 MONEYTYPE
/
DATETIMETYPE
/
FLT8TYPE
/
MONEY4TYPE
/
INT8TYPE


TYPE_INFO = FIXEDLENTYPE
           /
          (VARLENTYPE TYPE_VARLEN [COLLATION])
         /
        (VARLENTYPE TYPE_VARLEN [PRECISION SCALE])
       /
      (VARLENTYPE SCALE) ; (introduced in TDS 7.3)
     /
    VARLENTYPE
   ; (introduced in TDS 7.3)
  /
 (PARTLENTYPE
[USHORTMAXLEN]
[COLLATION]
[XML_INFO]
[UDT_INFO])


*/

enum data_type
{
	dt_null = 0x1f, ///< Null
	dt_int1 = 0x30, ///< 1 byte
	dt_bit = 0x32, ///< 1 byte
	dt_int2 = 0x34, ///< 2 bytes
	dt_int4 = 0x38, ///< 4 bytes
	dt_datetim4 = 0x3a, ///< 4 bytes
	dt_flt4 = 0x3b, ///< 4 bytes
	dt_money = 0x3c, ///< 8 bytes
	dt_datetime = 0x3d, ///< 8 bytes
	dt_flt8 = 0x3e, ///< 8 bytes
	dt_money4 = 0x7a, ///< 4 bytes
	dt_int8 = 0x7f, ///< 8 bytes
	dt_intn = 0x26, ///< 1, 2, 4 or 8 bytes
};

struct column_info
{
	enum type {
		ct_udt = 0x0000,
	};
#pragma pack(push, 1)
	struct {
		uint32_t user_type;
		struct {
			unsigned f_null:1;
			unsigned f_casesen:1;
			unsigned us_updateable:2;
			unsigned f_identity:1;
			unsigned f_computed:1;
			unsigned us_reserved_odbc:2;
			unsigned f_fixedlen_clr_type:1;
			unsigned f_reserved_bit:1;
			unsigned f_sparsecolumnset:1;
			unsigned us_reserved2:2;
			unsigned f_hidden:1;
			unsigned f_key:1;
			unsigned f_nullable_unknown:1;
		} flags;
	} fixed1;
#pragma pack(pop)

	uint8_t type;

	uint32_t length;
#pragma pack(pop)

	//TYPE_INFO;

	std::vector<std::string> table_name;
	std::string col_name;
	bool decode(buffer& input);
};

struct frame_token_colmetadata
{
	typedef std::deque<column_info> info_type;
	info_type info;

	bool decode(buffer& input);
	void clear();
};

struct column_data
{
	//column_data() { }
	//virtual ~column_data() { }
	bool decode(const column_info& info, buffer& input);

	union
	{
		int64_t v_bigint;
		double v_real;
	} data;

	std::string raw;
};

//typedef std::tr1::shared_ptr<shared_ptr> column_data_ptr;

struct frame_token_row
{
	//std::deque<column_data_ptr> columns;
	std::deque<column_data> data;

	bool decode(const frame_token_colmetadata& meta, buffer& input);
};

struct frame_response
{
	std::deque<frame_token_error> errors;
	bool auth_success;
	frame_token_colmetadata columns_info;
	std::deque<frame_token_row> rows;

	frame_response();

	bool decode(buffer& input);
};

struct frame_trans_request
{
	/*
	all_headers;
	request_type;
	request_payload
	*/
};

} // namespace tds
