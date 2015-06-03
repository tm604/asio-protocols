#include <net/asio/amqp.h>

namespace std {

string to_string(const net::amqp::connection_details &details)
{
	string s = details.protocol() + "://";
	if(details.have_auth()) {
		s += details.username() + ":" + details.password() + "@";
	}
	s += details.host();
	if(details.have_port()) {
		s += ":" + to_string(details.port());
	}
	s += "/" + details.vhost();
	return s;
}

};

