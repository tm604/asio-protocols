#include "catch.hpp"
#include <boost/algorithm/string.hpp>

#include "net/asio/http.h"
#include "net/asio/tcp.h"

#include <boost/asio.hpp>

using namespace std;
using namespace net::http;

SCENARIO("TCP listener auto port assignment", "[transport],[tcp]") {
	boost::asio::io_service srv;
	GIVEN("a TCP server") {
		auto hs = net::asio::tcp::server::create(srv);
		WHEN("we listen with port 0") {
			auto f = hs->listen("localhost", 0);
			THEN("we are assigned a non-zero port") {
				f->on_done([&srv, hs](bool is_listening) {
					CHECK(is_listening);
					CHECK(hs->listening_port() > 0);
					srv.stop();
				});
				REQUIRE_NOTHROW(srv.run());
			}
		}
	}
}

SCENARIO("TCP client can connect to TCP server", "[transport],[tcp]") {
	boost::asio::io_service srv;
	GIVEN("a TCP server and client") {
		auto ts = net::asio::tcp::server::create(srv);
		auto tc = net::asio::tcp::client::create(srv);
		WHEN("we start listening") {
			auto sf = ts->listen("localhost", 0);
			THEN("we can connect with the client") {
				auto cf = sf->then([&srv, ts, tc](bool is_listening) {
					CHECK(is_listening);
					CHECK(ts->listening_port() > 0);
					return tc->connect("localhost", ts->listening_port());
				})->on_done([&srv](bool is_connected) {
					CHECK(is_connected);
					srv.stop();
				});
				REQUIRE_NOTHROW(srv.run());
			}
		}
	}
}

using tcp_pair = std::pair<
	std::shared_ptr<net::asio::tcp::server>,
	std::shared_ptr<net::asio::tcp::stream>
>;

std::shared_ptr<
	cps::future<
		tcp_pair
	>
>
tcp_conn(
	boost::asio::io_service &srv
)
{
	auto ts = net::asio::tcp::server::create(srv);
	auto tc = net::asio::tcp::client::create(srv);
	return ts->listen(
		"localhost",
		0
	)->then([&srv, ts, tc](bool is_listening) {
		REQUIRE(is_listening);
		REQUIRE(ts->listening_port() > 0);
		return tc->connect("localhost", ts->listening_port());
	})->then([&srv, ts, tc](bool is_connected) {
		REQUIRE(is_connected);
		auto f = cps::future<tcp_pair>::create_shared();
		return is_connected ? f->done(
			std::make_pair(ts->first_connection(), tc)
		) : f->fail("did not connect");
	});
}

SCENARIO("TCP client write is received by TCP server", "[transport],[tcp]") {
	boost::asio::io_service srv;
	GIVEN("a TCP client connected to a server") {
		auto f = tcp_conn(srv);
		WHEN("we write data from the client") {
			std::string data;
			std::string txt { "some text from the client" };
			f->on_done([&srv](const tcp_pair &tp) {
				auto ts = tp.first;
				auto tc = tp.second;
				auto target_size = txt.size();
				ts->read_handler([data, target_size](const std::string &in) {
					data += in;
					if(data.size() >= target_size) {
						srv.stop();
					}
				});
				tc->write(
					txt
				)->on_done([&srv, ts](int) {
					THEN("we receive it at the server") {
						srv.stop();
					}
				});
			});
			// f2->on_ready([f](const 
			REQUIRE_NOTHROW(srv.run());
		}
	}
}
