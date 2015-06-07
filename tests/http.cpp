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

request req_from_string(string in)
{
	request r;
	boost::algorithm::trim_left(in);
	r.parse_data(
		boost::regex_replace(
			in,
			boost::regex("\n"),
			"\r\n",
			boost::match_default | boost::format_all
		)
	);
	return r;
}

response res_from_string(string in)
{
	response r;
	boost::algorithm::trim_left(in);
	r.parse_data(
		boost::regex_replace(
			in,
			boost::regex("\n"),
			"\r\n",
			boost::match_default | boost::format_all
		)
	);
	return r;
}

BOOST_AUTO_TEST_CASE(http_request)
{
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
	{
		auto r = req_from_string(R"(
GET / HTTP/1.1
Host: example.com
Server: nginx

)");
		BOOST_CHECK_EQUAL(r.method(), "GET");
		BOOST_CHECK_EQUAL(r.request_path(), "/");
		BOOST_CHECK_EQUAL(r.version(), "HTTP/1.1");
		BOOST_CHECK_EQUAL(r.header_count(), 2);
		BOOST_CHECK_EQUAL(r.header_value("Host"), "example.com");
		BOOST_CHECK_EQUAL(r.header_value("Server"), "nginx");
	}
}

BOOST_AUTO_TEST_CASE(http_response)
{
	{
		response r;
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
	{
		auto r = res_from_string(R"(
HTTP/1.1 200 OK
Server: nginx
Last-Modified: yesterday
Content-Length: 0

)");
		BOOST_CHECK_EQUAL(r.version(), "HTTP/1.1");
		BOOST_CHECK_EQUAL(r.status_code(), 200);
		BOOST_CHECK_EQUAL(r.status_message(), "OK");
		BOOST_CHECK_EQUAL(r.header_count(), 3);
		BOOST_CHECK_EQUAL(r.header_value("Server"), "nginx");
		BOOST_CHECK_EQUAL(r.header_value("Last-Modified"), "yesterday");
		BOOST_CHECK_EQUAL(r.header_value("Content-Length"), "0");
	}
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

