#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>
#include <cps/future.h>

namespace net {
namespace asio {
namespace tcp {

/**
 * A stream represents a TCP link between two endpoints. There is one stream
 * for the server->client connection, and another for client->server.
 *
 * A stream is two-way: it can read and write.
 * 
 */
class stream : public std::enable_shared_from_this<stream> {
public:
	stream(
		std::shared_ptr<boost::asio::ip::tcp::socket> &&socket,
	):socket_(std::move(socket))
	{
	}

	virtual ~stream() = default;

	/**
	 * Attaches the given code as a sink.
	 */
	void sink(std::function<void(const std::string &)> code) {
		auto f = cps::future<bool>::create_shared()->done(true);
		on_read_ = [code, f](const std::string &in) {
			code(in);
			return f;
		};
	}

	/**
	 * Queues some outgoing data.
	 * Will return a future that resolves with the number of bytes actually written.
	 * If that number is less than the original length, it means that we think we wrote
	 * some data, but the connection was interrupted before the end.
	 */
	std::shared_ptr<cps::future<int>>
	write(const std::string &in)
	{
		{
			std::lock_guard<std::mutex> guard { mutex_ };
			outgoing_.push(in);
		}
		check_outgoing();
	}

	void check_outgoing()
	{
		std::shared_ptr<std::string> next;
		{
			std::lock_guard<std::mutex> guard { mutex_ };
			if(outgoing_.empty()) return;

			/* split from here into a separate _locked version of this function */
			if(sending_) return;

			if(outgoing_.empty())
				throw logic_error("outgoing is empty, we should not have been called");

			sending_ = true;
			next = std::make_shared<std::string>(outgoing_.front());
			outgoing_.pop();
		}
		auto self = shared_from_this();
		socket_->async_write(
			boost::asio::const_buffer(next),
			[self, next](const boost::system::error_code &ec, size_t written) {
				if(ec) {
					self->write_error(ec.message());
				} else {
					std::lock_guard<std::mutex> guard { mutex_ };
					self->sending_ = false;
				}
				self->check_outgoing();
			}
		);
	}

	void
	read_handler()
	{
		auto storage = std::make_shared<std::vector<char>>(4096);
		auto self = shared_from_this();
		socket_->async_read_some(
			boost::asio::buffer(&((*storage)[0]), storage->size()),
			[self, storage](
				const boost::system::error_code &ec,
				size_t len
			) {
				if(ec) {
					self->connection_error(ec.message());
				} else {
					storage->resize(len);
					self->incoming_.insert(end(self->incoming_), begin(*storage), end(*storage));
					size_t available = self->incoming_.size();
					size_t processed = 0;
					char *ptr = &((self->incoming_)[0]);
					while(available > 0) {
						size_t parsed = self->conn_->parse(ptr, available);
						if(parsed == 0) break;
						ptr += parsed;
						available -= parsed;
						processed += parsed;
					}
					self->incoming_.erase(begin(self->incoming_), begin(self->incoming_) + processed);
					/* Defer callback until we can read again */
					self->read_handler();
				}
			}
		);
	}

	std::shared_ptr<boost::asio::ip::tcp::socket> socket() { return socket_; }

private:
	std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
	std::function<std::shared_ptr<cps::future<bool>>(const std::string &)> on_read_;
	std::queue<std::string> outgoing_;
};

class client : public stream {
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
	):service_(service)
	{
	}

virtual ~client() { }

	std::shared_ptr<cps::future<stream>>
	connect(
		const std::string &hostname,
		const uint16_t port
	)
	{
		using boost::asio::ip::tcp;
		auto self = shared_from_this();
		auto f = cps::future<bool>::create_shared();

		try {
			auto resolver = std::make_shared<tcp::resolver>(service_);
			auto query = std::make_shared<tcp::resolver::query>(
				hostname,
				std::to_string(port)
			);
			auto socket = std::make_shared<tcp::socket>(service_);

			resolver->async_resolve(
				*query,
				[query, resolver, f, self, socket](const boost::system::error_code &ec, const tcp::resolver::iterator ei) {
					if(ec) {
						f->fail(ec.message());
					} else {
						boost::asio::async_connect(
							*(socket),
							ei,
							[self, f, socket](boost::system::error_code ec, tcp::resolver::iterator it) {
								if(ec) {
									f->fail(ec.message());
								} else {
									tcp::socket::non_blocking_io nb(true);
									socket->io_control(nb);
									f->done(std::make_shared<stream>(socket));
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
		}
		return f;
	}

private:
	boost::asio::io_service &service_;
	std::shared_ptr<stream> stream_;
};

class server : public std::enable_shared_from_this<server> {
public:
	static
	std::shared_ptr<server>
	create(
		boost::asio::io_service &srv
	)
	{
		return std::make_shared<server>(srv);
	}

	server(
		boost::asio::io_service &service
	):service_(service),
	  acceptor_(service),
	  listening_port_{ 0 }
	{
	}

	virtual ~server() = default;

	std::shared_ptr<cps::future<bool>>
	listen(
		const std::string &hostname,
		uint16_t port = 0)
	{
		using boost::asio::ip::tcp;
		auto self = shared_from_this();
		auto f = cps::future<bool>::create_shared();

		try {
			auto resolver = std::make_shared<tcp::resolver>(service_);
			auto query = std::make_shared<tcp::resolver::query>(
				hostname,
				std::to_string(port)
			);

			resolver->async_resolve(
				*query,
				[query, resolver, f, self](const boost::system::error_code &ec, const tcp::resolver::iterator ei) {
					if(ec) {
						f->fail(ec.message());
					} else {
						if(ec) {
							f->fail(ec.message());
						} else {
							auto &acc = self->acceptor_;
							acc.open(ei->endpoint().protocol());
							acc.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
							acc.bind(ei->endpoint());
							acc.listen(512);
							acc.non_blocking(true);
							self->listening_port_ = acc.local_endpoint().port();
							self->accept();
							f->done(true);
						}
					}
				}
			);
		} catch(const boost::system::system_error& e) {
			f->fail(e.what());
		} catch(const std::exception& e) {
			f->fail(e.what());
		}
		return f;
	}

	uint16_t listening_port() const { return listening_port_; }

	std::shared_ptr<stream> first_connection() { return connections_.at(0); }

	void accept() {
		auto self = shared_from_this();
		auto socket = std::make_shared<boost::asio::ip::tcp::socket>(service_);
		acceptor_.async_accept(
			*socket,
			[self, socket](const boost::system::error_code &ec) {
				if (!self->acceptor_.is_open()) {
					std::cerr << "This acceptor is not open, so we should not be called?\n";
					return;
				}

				if(ec) {
					std::cerr << "New connection\n";
					self->connections_.push_back(std::make_shared<stream>(std::move(socket), self));
				}
				self->accept();
			}
		);
	}

private:
	boost::asio::io_service &service_;
	boost::asio::ip::tcp::acceptor acceptor_;
	uint16_t listening_port_;
	std::vector<std::shared_ptr<stream>> connections_;
};

};
};
};

