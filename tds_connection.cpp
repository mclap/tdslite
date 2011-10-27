#include <arpa/inet.h>
#include <sys/types.h>

#include "tds_connection.h"

#include <vector>
#include <string>
#include <iterator>
#include <algorithm>

#include "debug.h"
#include "tds_frames.h"

namespace tds
{

connection::connection()
	: cn(0), last_packet_id(0)
{
}

connection::~connection()
{
	disconnect();
}

bool connection::connect(const char *host, int port)
{
	if (cn)
		return true;

	cn = net_create(host, port);
	if (cn)
		return true;

	return false;
}

void connection::disconnect()
{
	if (!cn)
		return;

	net_free(cn);
	cn = 0;
}

unsigned char connection::gen_packet_id()
{
	return ++last_packet_id;
}

bool connection::pull_response(frame_header& header, frame_response& resp)
{
	do
	{
		if (!header.pull(cn))
			return false;

		if (header.type != frame_header::table_response)
			return false;

		buffer body;
		if (!body.pull(cn, ntohs(header.ns_length) - sizeof(header)))
			return false;

		if (!resp.decode(body))
			return false;

	} while((header.status & frame_header::eom) != frame_header::eom);

	return true;
}

bool connection::pull_prelogin_response(frame_header& header, frame_prelogin& resp)
{
	do
	{
		if (!header.pull(cn))
			return false;

		/* from MS-TDS:
		 * A message sent by the client to set up context for login.
		 * The server responds to a client PRELOGIN message with a
		 * message of packet header type 0x04 and the packet data
		 * containing a PRELOGIN structure.
		 */
		if (header.type != frame_header::table_response) // sic!
			return false;

		buffer body;
		if (!body.pull(cn, ntohs(header.ns_length) - sizeof(header)))
			return false;

		if (!resp.decode(body))
			return false;

	} while((header.status & frame_header::eom) != frame_header::eom);

	return true;
}

bool connection::call_prelogin()
{
	frame_header header;
	frame_prelogin request;
	buffer buf;

	/* request */

	request.encode(buf);
	header.type = frame_header::pre_login;
	header.packet_id = gen_packet_id();
	header.ns_length = htons(buf.size() + sizeof(header));

	if (!header.push(cn))
		return false;

	if (!buf.push(cn))
		return false;

	/* response */
	frame_prelogin resp;
	if (!pull_prelogin_response(header, resp))
		return false;

	return true;
}

bool connection::call_login7(const std::string& dbhost, const std::string& dbuser, const std::string& dbpass, const std::string& dbname)
{
	frame_header header;
	frame_login7 request;

	/* request */

	request.client_host = "localhost";
	request.user_name = dbuser;
	request.user_pass = dbpass;
	request.app_name = "tdslite";
	request.server_name = dbhost;
	request.database = dbname;

	buffer tmp;
	request.encode(tmp);
	TP_DEBUG("tmp.size=%d", (int)tmp.size());

	header.type = frame_header::tds7_login;
	header.packet_id = gen_packet_id();
	header.ns_length = htons(tmp.size() + sizeof(header));

	buffer buf;
	buf.put(header);
	TP_DEBUG("buf.size=%d", (int)buf.size());

	buf.put(tmp);
	TP_DEBUG("buf.size=%d", (int)buf.size());

	if (!buf.push(cn))
		return false;

	/* response */
	frame_response resp;
	if (!pull_response(header, resp))
		return false;

	return resp.auth_success;
}

bool connection::call_sql_batch(const std::string& query)
{
	frame_header header;
	frame_sql_batch request;

	/* request */
	request.query = query;
	request.auto_commit = true;

	buffer tmp;
	request.encode(tmp);
	TP_DEBUG("tmp.size=%d", (int)tmp.size());

	header.type = frame_header::sql_batch;
	header.packet_id = gen_packet_id();
	header.ns_length = htons(tmp.size() + sizeof(header));

	buffer buf;
	buf.put(header);
	TP_DEBUG("buf.size=%d", (int)buf.size());

	buf.put(tmp);
	TP_DEBUG("buf.size=%d", (int)buf.size());

	if (!buf.push(cn))
		return false;

	/* response */
	frame_response resp;
	if (!pull_response(header, resp))
		return false;

	TP_DEBUG("errors: %d", resp.errors.size());

	std::deque<frame_token_error>::iterator iter = resp.errors.begin();
	for ( ; iter != resp.errors.end(); ++iter)
	{
		TP_DEBUG("error: %s", iter->error_text.c_str());
	}
	return true;
}

} /* namespace tds */
