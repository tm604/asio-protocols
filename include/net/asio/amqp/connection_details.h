#pragma once
#include <string>
#include <memory>
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>
#include <cps/future.h>

namespace net {
namespace amqp {

class connection_details {
public:
	connection_details(
		const std::string &host = "localhost",
		const std::string &user = "guest",
		const std::string &pass = "guest",
		const std::string &vhost = "/",
		const uint16_t port = 5672,
		const std::string &proto = "amqp"
	):protocol_{ proto },
	  host_{ host },
	  port_{ port },
	  user_{ user },
	  pass_{ pass },
	  vhost_{ vhost }
	{
	}

	static
	connection_details
	parse(const std::string &uri)
	{
		static const boost::regex re(
			"^(?<protocol>amqps?)://(?:(?<user>[^:]*):(?<password>[^@]*)@)?(?<host>[^:/]+)(?::(?<port>\\d+))?(?:/(?<vhost>.*))?$",
			boost::regex::icase
		);
		boost::smatch matches;
		if(!regex_match(uri, matches, re)) throw std::runtime_error("Failed to parse " + uri);
		std::string proto = matches["protocol"];
		std::string host = matches["host"];
		uint16_t port = std::string(matches["port"]).empty() ? port_for_proto(proto) : std::stoi(matches["port"]);
		std::string user = matches["user"];
		std::string pass = matches["password"];
		std::string vhost = matches["vhost"];
		return connection_details(
			host,
			user,
			pass,
			vhost,
			port,
			proto
		);
	}

	static uint16_t port_for_proto(const std::string &proto) {
		if(proto == "amqp") return 5672;
		if(proto == "amqps") return 5671;
		throw std::runtime_error("Unknown AMQP scheme " + proto);
	}

	uint16_t default_port() const { return port_for_proto(protocol_); }

	std::string host() const { return host_; }
	std::string vhost() const { return vhost_; }
	uint16_t port() const { return port_; }
	std::string username() const { return user_; }
	std::string password() const { return pass_; }
	std::string protocol() const { return protocol_; }

	bool have_auth() const {
		return user_.size() > 0;
	}

	bool have_port() const {
		return port_ != default_port();
	}

private:
	std::string protocol_;
	std::string host_;
	uint16_t port_;
	std::string user_;
	std::string pass_;
	std::string vhost_;
};

};

};