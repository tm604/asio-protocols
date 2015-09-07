#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <net/asio/http/connection.h>

namespace net {
namespace http {

class tls_connection : public connection {
	tls_connection(
		boost::asio::io_service &service,
		const std::string &hostname,
		uint16_t port
	):connection(service, hostname, port),
	  ctx_{ boost::asio::ssl::context::tlsv12 },
	  socket_(
		std::make_shared<
			boost::asio::ssl::stream<
				boost::asio::ip::tcp::socket
			>
		>(
			service,
			ctx_
		)
	  )
	{
		ctx_.set_default_verify_paths();
#if 1
		socket_->set_verify_mode(boost::asio::ssl::verify_none);
#else
		socket_->set_verify_mode(boost::asio::ssl::verify_peer);
		socket_->set_verify_callback(boost::asio::ssl::rfc2818_verification(host_));
#endif
	}

	std::shared_ptr<
		tls_connection
	>
	shared_from_this()
	{
		return std::dynamic_pointer_cast<
			tls_connection
		>(
			connection::shared_from_this()
		);
	}

	virtual std::shared_ptr<cps::future<bool>> post_connect() override {
		auto f = cps::future<bool>::create_shared("https post-connect");
		auto self = shared_from_this();
		socket_->async_handshake(
			boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client,
			[self, f](const boost::system::error_code &ec) {
				if(ec) {
					f->fail(ec.message());
				} else {
					f->done(true);
				}
			}
		);
		return f;
	}

	virtual boost::asio::ip::tcp::socket &socket() override { return *socket_; }
	virtual boost::asio::ip::tcp::socket &connection_socket() override { return socket_->lowest_layer(); }

private:
	boost::asio::ssl::context ctx_;
	std::shared_ptr<
		boost::asio::ssl::stream<
			boost::asio::ip::tcp::socket
		>
	> socket_;
};

};
};

