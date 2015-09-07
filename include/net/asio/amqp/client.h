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

	std::shared_ptr<cps::future<std::shared_ptr<net::amqp::connection>>>
	connect(
		const connection_details &cd
	)
	{
		using boost::asio::ip::tcp;
		auto self = shared_from_this();
		std::shared_ptr<cps::future<std::shared_ptr<net::amqp::connection>>>
			f = cps::future<std::shared_ptr<net::amqp::connection>>::create_shared("MQ connection to " + cd.host());

		try {
			auto resolver = std::make_shared<tcp::resolver>(service_);
			auto query = std::make_shared<tcp::resolver::query>(
				cd.host(),
				std::to_string(cd.port())
			);
			auto socket_ = std::make_shared<tcp::socket>(service_);

			resolver->async_resolve(
				*query,
				[query, socket_, resolver, f, cd, self](boost::system::error_code ec, tcp::resolver::iterator ei) {
					if(ec) {
						f->fail(ec.message());
					} else {
						boost::asio::async_connect(
							*socket_,
							ei,
							[socket_, f, cd, self](boost::system::error_code ec, tcp::resolver::iterator it) {
								if(ec) {
									f->fail(ec.message());
								} else {
									tcp::socket::non_blocking_io nb(true);
									socket_->io_control(nb);
									auto mc = std::make_shared<net::amqp::connection>(socket_, self->service_, cd);
									mc->setup();
									f->done(mc);
								}
							}
						);
					}
				}
			);
		} catch(const boost::system::system_error& e) {
			f->fail(e.what());
		} catch(const std::exception& e) {
			f->fail(e.what());
		} catch(const std::string& e) {
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
	string to_string(const net::amqp::connection_details &details);
};

