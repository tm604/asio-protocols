#include "catch.hpp"
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

/** Converts a string from a test into a request */
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

/** Converts a string from a test into a response */
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

SCENARIO("http request", "[http]") {
	GIVEN("an empty request") {
		request r;
		vector<pair<string, string>> seen_headers;
		r.on_header_added.connect([&seen_headers](const header &h) {
			seen_headers.push_back({ h.key(), h.value() });
		});
		WHEN("we add a header") {
			r << header("some-header", "x");
			THEN("header count increases") {
				REQUIRE(seen_headers.size() == 1);
			}
			THEN("our callback was triggered") {
				CHECK(seen_headers.size() == 1);
				CHECK(seen_headers[0].first == "Some-Header");
				CHECK(seen_headers[0].second == "x");
			}
			WHEN("we add another header") {
				r << header("other-header", "y");
				THEN("header count is now 2") {
					CHECK(seen_headers.size() == 2);
				}
				THEN("value matches for both headers") {
					CHECK(r.header_value("some-header") == "x");
					CHECK(r.header_value("other-header") == "y");
				}
				THEN("callback was called again") {
					CHECK(seen_headers.size() == 2);
					CHECK(seen_headers[1].first == "Other-Header");
					CHECK(seen_headers[1].second == "y");
				}
			}
		}
	}
	GIVEN("a request from a string") {
		auto r = req_from_string(R"(
GET / HTTP/1.1
Host: example.com
Server: nginx

)");
		CHECK(r.method() == "GET");
		CHECK(r.request_path() == "/");
		CHECK(r.version() == "HTTP/1.1");
		CHECK(r.header_count() == 2);
		CHECK(r.header_value("Host") == "example.com");
		CHECK(r.header_value("Server") == "nginx");
	}
}

SCENARIO("http response", "[http]") {
	GIVEN("an empty response") {
		response r;
		vector<pair<string, string>> seen_headers;
		r.on_header_added.connect([&seen_headers](const header &h) {
			seen_headers.push_back({ h.key(), h.value() });
		});
		WHEN("we add a header") {
			r << header("some-header", "x");
			THEN("header count increases") {
				REQUIRE(seen_headers.size() == 1);
			}
			THEN("our callback was triggered") {
				CHECK(seen_headers.size() == 1);
				CHECK(seen_headers[0].first == "Some-Header");
				CHECK(seen_headers[0].second == "x");
			}
			WHEN("we add another header") {
				r << header("other-header", "y");
				THEN("header count is now 2") {
					CHECK(seen_headers.size() == 2);
				}
				THEN("value matches for both headers") {
					CHECK(r.header_value("some-header") == "x");
					CHECK(r.header_value("other-header") == "y");
				}
				THEN("callback was called again") {
					CHECK(seen_headers.size() == 2);
					CHECK(seen_headers[1].first == "Other-Header");
					CHECK(seen_headers[1].second == "y");
				}
			}
		}
	}
	GIVEN("a response from a string") {
		auto r = res_from_string(R"(
HTTP/1.1 200 OK
Server: nginx
Last-Modified: yesterday
Content-Length: 0

)");
		CHECK(r.version() == "HTTP/1.1");
		CHECK(r.status_code() == 200);
		CHECK(r.status_message() == "OK");
		CHECK(r.header_count() == 3);
		CHECK(r.header_value("Server") == "nginx");
		CHECK(r.header_value("Last-Modified") == "yesterday");
		CHECK(r.header_value("Content-Length") == "0");
	}
}

SCENARIO("header normalisation", "[http]") {
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
		CHECK(header(c.first, "x").key() == c.second);
	}
}

