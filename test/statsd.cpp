#include "catch.hpp"
#include <map>
#include <iostream>
#include <functional>

#include <boost/asio.hpp>

// #include <net/asio/statsd.h>

namespace net {
namespace protocol {

namespace statsd {

/** Represents a single item stored on the server */
class stored_item {
public:
	bool operator<(int64_t v) const { return value_ < v; }
	bool operator>(int64_t v) const { return value_ > v; }
	bool operator==(int64_t v) const { return value_ == v; }

	stored_item &operator=(int64_t v) { value_ = v; return *this; }
	stored_item &operator++() { ++value_; }
	stored_item &operator++(int) { operator++(); return *this; }
	stored_item &operator--() { --value_; }
	stored_item &operator--(int) { operator--(); return *this; }

private:
	std::string key_;
	int64_t value_;
};

class gauge : public stored_item { };
class counter : public stored_item { };
class timer : public stored_item { };
class meter : public stored_item { };

#if 0
class x {
public:

	proto << key_ << ":" << value_ << "|" << type_;
	// key_ is a vector of strings
	// value is int64_t
	std::string key_;
	int64_t value_;
	char type_;
};
#endif

/** The server interface */
class server {
public:
	server() = default;

	/** Returns true if we have any information about the given key */
	bool has(const std::string &k) const { return items_.count(k) > 0; }
	const stored_item &key(const std::string &k) const { return items_.at(k); }

private:
	std::map<std::string, stored_item> items_;
};

/** An item that can be updated */
class item {
public:
	item(const std::string &k):key_(k) { }

	item &operator=(int64_t v) { value_ = v; return *this; }
	item &operator++() { ++value_; }
	item &operator++(int) { operator++(); return *this; }
	item &operator--() { --value_; }
	item &operator--(int) { operator--(); return *this; }

private:
	std::string key_;
	int64_t value_;
};

class client {
public:
	client(server &s):srv_(s) { }
	item key(const std::string &s) { }

private:
	server &srv_;
};

};

};
};

class statsd_server : public std::enable_shared_from_this<statsd_server> {
public:
	statsd_server(
		boost::asio::io_service &service
	):endpoint_{ boost::asio::ip::udp::v4(), 0 },
	  socket_{ service, endpoint_ },
	  max_length_{1024}
	{
	}

	uint16_t port() const { return endpoint_.port(); }

	void on_packet(const std::string &in) {
		std::cout << "Incoming packet: " << in << std::endl;
	}

	/**
	 * statsd server - continuously accepts incoming message packets
	 */
	void
	accept_next()
	{
		auto self = shared_from_this();
		/** Incoming data storage */
		auto storage = std::make_shared<std::vector<char>>(max_length_);
		socket_.async_receive_from(
			boost::asio::buffer(&((*storage)[0]), storage->size()),
			endpoint_,
			/* We want to transfer std::unique_ptr ownership to our lambda, but
			 * might not have C++14 semantics available
			 */
			[self, storage](
				const boost::system::error_code &ec,
				size_t bytes
			) {
				if(ec) {
					std::cerr << "Had an error while waiting for next packet: " << ec.message() << std::endl;
				} else if(bytes > 0) {
					self->on_packet(std::string { storage->cbegin(), storage->cend() });
				}
				self->accept_next();
			}
		);
	}

private:
	boost::asio::ip::udp::endpoint endpoint_;
	boost::asio::ip::udp::socket socket_;
	size_t max_length_;
};

/**
 * statsd client - just sends out a packet for each stat
 */
class statsd_client : public std::enable_shared_from_this<statsd_client> {
public:
	statsd_client(
		boost::asio::io_service &service
	):socket_(service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
	  max_length_{4096}
	{
	}

	void send_packet(const std::string &in)
	{
		auto self = shared_from_this();
		auto storage = std::make_shared<std::vector<char>>(in.cbegin(), in.cend());
		auto length = storage->size();
		socket_.async_send_to(
			boost::asio::buffer(&((*storage)[0]), storage->size()),
			endpoint_,
			[self, length, storage](
				const boost::system::error_code &ec,
				size_t bytes
			) {
				if(ec) {
					std::cerr << "Error sending: " << ec.message() << std::endl;
				} else if(bytes == length) {
					std::cout << "Sent all expected data\n";
				} else {
					std::cerr << "Sent fewer bytes than we expected\n";
				}
			}
		);
	}
private:
	boost::asio::ip::udp::socket socket_;
	boost::asio::ip::udp::endpoint endpoint_;
	size_t max_length_;
};

#if 0
SCENARIO("statsd server") {
	using namespace net::protocol;
	GIVEN("a statsd server instance") {
		auto srv = statsd::server();
		auto cli = statsd::client(srv);
		WHEN("we inc a key") {
			cli.key("some.key")++;
			THEN("the value is correct") {
				CHECK(srv.has("some.key"));
				CHECK(srv.key("some.key") == 1);
			}
		}
		WHEN("we dec that key") {
			cli.key("some.key")--;
			THEN("the value is correct") {
				CHECK(srv.has("some.key"));
				CHECK(srv.key("some.key") == 0);
			}
		}
	}
}

SCENARIO("UDP handling", "[statsd][udp]") {
	boost::asio::io_service iosrv;
	GIVEN("a statsd server instance") {
		auto srv = statsd_server(iosrv);
		CHECK(srv.port() > 0);
		std::cout << srv.port() << "\n";
		iosrv.run();
	}
}
#endif
