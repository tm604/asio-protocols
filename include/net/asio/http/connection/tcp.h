#pragma once
#include <iostream>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/read_until.hpp>
#include <net/asio/http/connection.h>
#include <net/asio/http/connection_pool.h>

namespace net {
namespace http {

class tcp : public connection {
public:
	tcp(
		boost::asio::io_service &service,
		connection_pool &pool,
		const std::string &hostname,
		uint16_t port
	):connection(service, pool, hostname, port),
	  socket_(std::make_shared<boost::asio::ip::tcp::socket>(service))
	{
	}

	virtual ~tcp() {
		// std::cerr << "~tcp conn " << (void *)this << "\n";
	}

	std::shared_ptr<
		tcp
	>
	shared_from_this()
	{
		return std::dynamic_pointer_cast<
			tcp
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
		auto f = cps::future<bool>::create_shared("http connect to " + hostname_ + ":" + std::to_string(port_));
		auto sock = socket_;
		auto self = shared_from_this();
		boost::asio::async_connect(
			*sock,
			ei,
			[self, sock, f](const boost::system::error_code &ec, const boost::asio::ip::tcp::resolver::iterator &) {
				if(ec) {
					self->close();
					if(!f->is_ready())
						f->fail("Connect failed: " + ec.message());
				} else {
					boost::asio::ip::tcp::socket::non_blocking_io nb(true);
					sock->io_control(nb);
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
		auto f = cps::future<size_t>::create_shared("http write to " + hostname_ + ":" + std::to_string(port_));
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
		auto f = cps::future<std::string>::create_shared("http read_delim from " + hostname_ + ":" + std::to_string(port_));
		auto self = shared_from_this();
		auto delim_size = delim.size();
		boost::asio::async_read_until(
			*socket_,
			*in_,
			delim,
			[self, f, delim_size](const boost::system::error_code &ec, size_t bytes) {
				// std::cout << "Read until - now have " << bytes << " bytes\n";
				if(ec) {
					// std::cerr << "Error received during read_delimited: " << ec.message() << "\n";
					self->close();
					if(!f->is_ready())
						f->fail(ec.message());
				} else {
					auto b = self->in_->data();
					auto start = boost::asio::buffers_begin(b);
					if(bytes < delim_size) {
						// std::cerr << "Received fewer bytes than expected in read_delimited\n";
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
		auto f = cps::future<std::string>::create_shared("http read(" + std::to_string(wanted) + " from " + hostname_ + ":" + std::to_string(port_));
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
		auto f = cps::future<bool>::create_shared("http post-connect for " + hostname_ + ":" + std::to_string(port_));
		extend_timer();
		handle_response();
		f->done(true);
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
		boost::system::error_code ec;
		socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		if(ec) {
			// std::cerr << "Failed to shut down HTTP socket: " << ec.message() << "\n";
		}
		socket_->close(ec);
		if(ec) {
			// std::cerr << "Failed to close: " << ec.message() << "\n";
		}
	}

private:
	std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
};

};
};
