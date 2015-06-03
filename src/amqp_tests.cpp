
#include "stdafx.h"

#define BOOST_TEST_MAIN
#if !defined( WIN32 )
    #define BOOST_TEST_DYN_LINK
#endif
#define BOOST_TEST_MODULE AMQPTests

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include "net/amqp.h"
#include "net/amqp/client.h"
#include "Log.h"

std::string
normalise(const std::string &in)
{
	std::string uri = in;
	boost::algorithm::trim(uri);
	if(uri.substr(uri.size() - 1, 1) != "/") uri += "/";
	return uri;
}

BOOST_AUTO_TEST_CASE(amqp_uri)
{
	using namespace std;
	auto cd = net::amqp::connection_details();
	auto cases = vector<string> {
		"amqp://u:p@somehost.example.com/vh",
		"amqps://u:p@somehost.example.com/vh",
		"amqp://somehost.example.com/vh",
		"amqp://somehost.example.com:5656/vh",
		"amqp://somehost.example.com/"
	};
	for(auto &uri : cases) {
		DEBUG << "Testing [" << uri << "]";
		BOOST_CHECK(cd.from_uri(uri));
		BOOST_CHECK(to_string(cd) == uri);
		if(to_string(cd) != uri) ERROR << "Expected [" << uri << "], had [" << to_string(cd) << "]";
	}
}

BOOST_AUTO_TEST_CASE(amqp_connect)
{
    boost::asio::io_service srv;
	auto amqp = net::amqp::client::create(srv);
	amqp->connect("localhost", 5672);
    srv.run();
}

