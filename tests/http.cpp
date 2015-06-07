#define BOOST_TEST_MAIN
#if !defined( WIN32 )
    #define BOOST_TEST_DYN_LINK
#endif
#define BOOST_TEST_MODULE HTTPTests

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include "net/asio/http.h"
#include "Log.h"

using namespace std;
using namespace net::http;

std::string
normalise(const std::string &in)
{
	std::string uri = in;
	boost::algorithm::trim(uri);
	if(uri.substr(uri.size() - 1, 1) != "/") uri += "/";
	return uri;
}

BOOST_AUTO_TEST_CASE(http_request)
{
	request r;
	vector<pair<string, string>> seen_headers;
	r.on_header_added.connect([&seen_headers](const header &h) {
		seen_headers.push_back({ h.key(), h.value() });
	});
	r << header("some-header", "x");
	BOOST_CHECK_EQUAL(seen_headers.size(), 1);
	r << header("other-header", "y");
	BOOST_CHECK_EQUAL(seen_headers.size(), 2);
	BOOST_CHECK_EQUAL(r.header_value("some-header"), "x");
	BOOST_CHECK_EQUAL(seen_headers[0].first, "Some-Header");
	BOOST_CHECK_EQUAL(seen_headers[0].second, "x");
	BOOST_CHECK_EQUAL(r.header_value("other-header"), "y");
	BOOST_CHECK_EQUAL(seen_headers[1].first, "Other-Header");
	BOOST_CHECK_EQUAL(seen_headers[1].second, "y");
}

BOOST_AUTO_TEST_CASE(header_normalization)
{
	auto cases = vector<pair<string, string>> {
		{ "some-header", "Some-Header" },
		{ "single", "Single" },
		{ "x-more-info", "X-More-Info" },
		{ "x-mOrE-iNfo", "X-More-Info" },
		{ "-hypheN-prEfIxed", "-Hyphen-Prefixed" },
		{ "double--hyphen", "Double--Hyphen" },
		{ ":http", ":http" }
	};
	for(auto &c : cases) {
		BOOST_CHECK_EQUAL(header(c.first, "x").key(), c.second);
	}
}

