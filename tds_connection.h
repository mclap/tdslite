#pragma once

#include "net.h"
#include "tds_frames.h"

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
	bool call_login7();

private:
	net_t cn;
	unsigned char last_packet_id;

	unsigned char gen_packet_id();

	bool pull_response(frame_header& header, buffer& body);
};

} /* namespace tds */
