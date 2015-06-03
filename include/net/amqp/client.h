#pragma once

#include <boost/regex.hpp>

#include <net/amqp.h>
#include <cps/future.h>

namespace net {
namespace amqp {

class connection_details {
public:
	connection_details(
	):protocol_{ "amqp" },
	  host_{ "localhost" },
	  port_{ 5672 },
	  user_{ "guest" },
	  pass_{ "guest" },
	  vhost_{ "/" }
	{
	}

	bool
	from_uri(const std::string &uri)
	{
		static const boost::regex re(
			"^(?<protocol>amqps?)://(?:(?<user>[^:]*):(?<password>[^@]*)@)?(?<host>[^:/]+)(?::(?<port>\\d+))?(?:/(?<vhost>.*))?$",
			boost::regex::icase
		);
		boost::smatch matches; 
		if(!regex_match(uri, matches, re)) return false;
		protocol_ = matches["protocol"];
		host_ = matches["host"];
		if(!std::string(matches["port"]).empty()) {
			port_ = std::stoi(matches["port"]);
		} else {
			port_ = default_port();
		}
		user_ = matches["user"];
		pass_ = matches["password"];
		vhost_ = matches["vhost"];
		return true;
	}

	uint16_t default_port() const {
		if(protocol_ == "amqp") return 5672;
		if(protocol_ == "amqps") return 5671;
		throw new std::runtime_error("Unknown AMQP scheme " + protocol_);
	}

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

class client:public std::enable_shared_from_this<client> {
public:

	static
	std::shared_ptr<client>
	create(
		boost::asio::io_service &srv
	)
	{
		return std::make_shared<client>(srv);
	}

	client(
		boost::asio::io_service &service
	):service_( service )
	{
	}

virtual ~client() { }

	cps::future::ptr
	connect(
		std::string host = "localhost",
		uint16_t port = 5672
	)
	{
		using boost::asio::ip::tcp;
		auto self = shared_from_this();
		auto f = cps::future::create();

		try {
			auto resolver = std::make_shared<tcp::resolver>(service_);
			auto query = std::make_shared<tcp::resolver::query>(
				host,
				std::to_string(port)	
			);
			auto socket_ = std::make_shared<tcp::socket>(service_);
			DEBUG << "Resolving RabbitMQ server";

			resolver->async_resolve(
				*query,
				[query, socket_, resolver, f, host, port, self](boost::system::error_code ec, tcp::resolver::iterator ei) {
					if(ec) {
						ERROR << "Error resolving AMQP server address: " << ec;
						f->fail(ec.message());
					} else {
						DEBUG << "Connecting to AMQP server on " << host << ":" << port;
						boost::asio::async_connect(
							*socket_,
							ei,
							[socket_, f, host, port, self](boost::system::error_code ec, tcp::resolver::iterator it) {
								if(ec) {
									ERROR << "Had error: " << ec;
									f->fail(ec.message());
								} else {
									DEBUG << "Connected to AMQP server";
									tcp::socket::non_blocking_io nb(true);
									socket_->io_control(nb);
									f->done();
								}
							}
						);
					}
				}
			);
		} catch(boost::system::system_error& e) {
			ERROR << "System error while attempting to connect to AMQP server: " << e.what();
			f->fail(e.what());
		} catch(std::exception& e) {
			std::cerr << "General exception: " << e.what() << std::endl;
			f->fail(e.what());
		} catch(std::string& e) {
			std::cerr << "Threw string: " << e << std::endl;
			f->fail(e);
		}
		return f;
	}

private:
    boost::asio::io_service &service_;
};

};
};

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

