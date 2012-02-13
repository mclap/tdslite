#pragma once

#include "net.h"
#include "tds_frames.h"
#include "frame_response.h"

namespace tds
{

class connection
{
public:
	connection();
	~connection();

	bool connect(const char *host, int port);
	void disconnect();

	bool call_prelogin();
	bool call_login7(const std::string& dbhost, const std::string& dbuser, const std::string& dbpass, const std::string& dbname);
	bool call_sql_batch(const std::string& query);

private:
	net_t cn;
	unsigned char last_packet_id;

	unsigned char gen_packet_id();

	bool pull_response(frame_header& header, frame_response& resp);
	bool pull_prelogin_response(frame_header& header, frame_prelogin& resp);
};

} /* namespace tds */
