#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <net/asio/http/connection.h>
#include <net/asio/http/connection_pool.h>

namespace net {
namespace http {

class tls : public connection {
public:
	tls(
		boost::asio::io_service &service,
		connection_pool &pool,
		const std::string &hostname,
		uint16_t port
	):connection(service, pool, hostname, port),
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

	virtual ~tls() {
		// std::cerr << "~tls conn " << (void *)this << "\n";
	}

	std::shared_ptr<
		tls
	>
	shared_from_this()
	{
		return std::dynamic_pointer_cast<
			tls
		>(
			connection::shared_from_this()
		);
	}

	virtual
	std::shared_ptr<
		cps::future<
			bool
		>
	>
	connect(
		const boost::asio::ip::tcp::resolver::iterator &ei
	) override
	{
		auto f = cps::future<bool>::create_shared("https connect to " + hostname_ + ":" + std::to_string(port_));
		auto sock = socket_;
		// std::cout << "will connect\n";
		auto self = shared_from_this();
		boost::asio::async_connect(
			socket_->lowest_layer(),
			ei,
			[self, sock, f](const boost::system::error_code &ec, const boost::asio::ip::tcp::resolver::iterator &) {
				// std::cout << "connect callback\n";
				if(ec) {
					self->close();
					if(!f->is_ready())
						f->fail("Connect failed: " + ec.message());
				} else {
				/*
					boost::asio::ip::tcp::socket::non_blocking_io nb(true);
					sock->lowest_layer().io_control(nb);
				*/
					f->done(true);
				}
			}
		);
		return f;
	}

	/**
	 * Attempts to write data to the underlying connection.
	 */
	virtual
	std::shared_ptr<
		cps::future<
			size_t
		>
	>
	write(std::shared_ptr<std::vector<char>> data) override
	{
		auto f = cps::future<size_t>::create_shared("https write to " + hostname_ + ":" + std::to_string(port_));
		auto self = shared_from_this();
		boost::asio::async_write(
			*socket_,
			boost::asio::buffer(*data),
			[self, data, f](const boost::system::error_code &ec, size_t bytes) {
				if(ec) {
					if(!f->is_ready())
						f->fail(ec.message());
					self->close();
				} else {
					f->done(bytes);
				}
			}
		);
		return f;
	}

	virtual
	std::shared_ptr<
		cps::future<
			std::string
		>
	>
	read_delimited(const std::string &delim) override
	{
		auto f = cps::future<std::string>::create_shared("https read_delim from " + hostname_ + ":" + std::to_string(port_));
		auto self = shared_from_this();
		auto delim_size = delim.size();
		boost::asio::async_read_until(
			*socket_,
			*in_,
			delim,
			[self, f, delim_size](const boost::system::error_code &ec, size_t bytes) {
				// std::cout << "Read until - now have " << bytes << " bytes\n";
				if(ec) {
					self->close();
					if(!f->is_ready())
						f->fail(ec.message());
				} else {
					auto b = self->in_->data();
					auto start = boost::asio::buffers_begin(b);
					if(bytes < delim_size) {
						if(!f->is_ready())
							f->fail("short read in read_delimited");
					} else {
						std::string str {
							start,
							start + static_cast<std::ptrdiff_t>(bytes - delim_size)
						};
						self->in_->consume(bytes);
						f->done(str);
					}
				}
			}
		);
		return f;
	}

	virtual
	std::shared_ptr<
		cps::future<
			std::string
		>
	>
	read(size_t wanted) override
	{
		auto f = cps::future<std::string>::create_shared("https read(" + std::to_string(wanted) + " from " + hostname_ + ":" + std::to_string(port_));
		if(in_->size() < wanted) {
			auto self = shared_from_this();
			boost::asio::async_read(
				*socket_,
				*in_,
				boost::asio::transfer_exactly(wanted - in_->size()),
				[self, f, wanted](const boost::system::error_code &ec, size_t bytes) {
					// std::cout << "Read " << bytes << "\n";
					if(ec) {
						self->close();
						if(!f->is_ready())
							f->fail("Error reading: " + ec.message());
					} else {
						auto b = self->in_->data();
						auto start = boost::asio::buffers_begin(b);
						std::string str {
							start,
							start + static_cast<std::ptrdiff_t>(wanted)
						};
						self->in_->consume(wanted);
						f->done(str);
					}
				}
			);
		} else {
			auto b = in_->data();
			auto start = boost::asio::buffers_begin(b);
			std::string str {
				start,
				start + static_cast<std::ptrdiff_t>(wanted)
			};
			in_->consume(wanted);
			f->done(str);
		}
		return f;
	}


	virtual std::shared_ptr<cps::future<bool>> post_connect() override {
		auto f = cps::future<bool>::create_shared("https post-connect for " + hostname_ + ":" + std::to_string(port_));
		auto self = shared_from_this();
		extend_timer();
		socket_->async_handshake(
			boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::client,
			[self, f](const boost::system::error_code &ec) {
				if(ec) {
					self->close();
					if(!f->is_ready())
						f->fail(ec.message());
				} else {
					self->extend_timer();
					self->handle_response();
					f->done(true);
				}
			}
		);
		return f;
	}

	virtual void close() override {
		if(already_closing()) {
			// std::cerr << "someone else is doing the close() for " << (void *)this << "\n";
			return;
		//} else {
		//	std::cerr << "we get to close() for " << (void *)this << "\n";
		}

		cancel_timer();
		remove();
		auto sock = socket_;
		boost::system::error_code ec;
		socket_->lowest_layer().cancel(ec);
		socket_->async_shutdown(
			[sock](const boost::system::error_code &ec) {
				if(ec) {
					// std::cerr << "Failed to shut down HTTPS socket: " << ec.message() << "\n";
				}
				sock->lowest_layer().close();
			}
		);
	}

	virtual void release() override {
		pool().release(shared_from_this());
	}

	/*
	virtual boost::asio::ip::tcp::socket &socket() override { return *socket_; }
	virtual boost::asio::ip::tcp::socket &connection_socket() override { return socket_->lowest_layer(); }
	*/

private:
	boost::asio::ssl::context ctx_;
	std::shared_ptr<
		boost::asio::ssl::stream<
			boost::asio::ip::tcp::socket
		>
	> socket_;
	std::atomic<bool> closed_;
};

};
};

