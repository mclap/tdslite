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

bool connection::pull_response(frame_header& header, buffer& body)
{
	header.pull(cn);
	body.pull(cn, ntohs(header.ns_length) - sizeof(header));
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

	header.push(cn);
	buf.push(cn);

	/* response */
	pull_response(header, buf);
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

	buf.push(cn);

	/* response */
	pull_response(header, buf);
}

} /* namespace tds */
