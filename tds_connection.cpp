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
	header.pull(cn);
	buf.pull(cn, ntohs(header.ns_length));
}

bool connection::call_login7()
{
#if 0
	frame_header header;
	frame_login7 request;

	/* request */

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
	header.pull(cn);
	buf.pull(cn, ntohs(header.ns_length));

#else

	const unsigned char sample[] = {
		0x10,0x01,0x00,0x90,0x00,0x00,0x01,0x00,0x88,0x00,0x00,0x00,0x02,0x00,0x09,0x72,
		0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
		0xE0,0x03,0x00,0x00,0xE0,0x01,0x00,0x00,0x09,0x04,0x00,0x00,0x5E,0x00,0x08,0x00,
		0x6E,0x00,0x02,0x00,0x72,0x00,0x00,0x00,0x72,0x00,0x07,0x00,0x80,0x00,0x00,0x00,
		0x80,0x00,0x00,0x00,0x80,0x00,0x04,0x00,0x88,0x00,0x00,0x00,0x88,0x00,0x00,0x00,
		0x00,0x50,0x8B,0xE2,0xB7,0x8F,0x88,0x00,0x00,0x00,0x88,0x00,0x00,0x00,0x88,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x73,0x00,0x6B,0x00,0x6F,0x00,0x73,0x00,0x74,0x00,
		0x6F,0x00,0x76,0x00,0x31,0x00,0x73,0x00,0x61,0x00,0x4F,0x00,0x53,0x00,0x51,0x00,
		0x4C,0x00,0x2D,0x00,0x33,0x00,0x32,0x00,0x4F,0x00,0x44,0x00,0x42,0x00,0x43,0x00,
		};

	unsigned char encoded[sizeof(sample)];

	buffer tmp;
	tmp.put(sample, sizeof(sample));
	frame_header header;
	frame_login7 request;

	tmp.fetch(header);
	request.decode(tmp);

	tmp.clear();

	tmp.put(header);
	TP_DEBUG("len1=%d, len2=%d", (int)(sizeof(sample)),(int)(tmp.size()));
	request.encode(tmp);
	TP_DEBUG("len1=%d, len2=%d", (int)(sizeof(sample)),(int)(tmp.size()));
	tmp.copy_to(0, tmp.size(), encoded);

	TP_DEBUG_DUMP(sample, sizeof(sample), "source, len=%d", (int)(sizeof(sample)));
	TP_DEBUG_DUMP(encoded, tmp.size(), "encoded, len=%d", (int)(tmp.size()));

#endif
}

} /* namespace tds */
