#include "catch.hpp"

#include <vector>
/**
 * Implements a basic ring buffer.
 * Underlying data storage is a std::vector.
 */
class ring_buffer {
public:
};

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
