#pragma once
#include <string>
#include <memory>
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/signals2.hpp>

namespace net {

namespace amqp {

class connection:public AMQP::ConnectionHandler, public std::enable_shared_from_this<connection> {
public:
    connection(
		std::shared_ptr<boost::asio::ip::tcp::socket> sock,
		boost::asio::io_service &srv,
		const connection_details &cd
	):active_{false},
	  writing_{false},
	  outgoing_{ },
	  incoming_{ },
	  strand_{ srv },
	  details_(cd),
	  sock_{ sock }
	{
	}

	void setup() {
		auto self = shared_from_this();
		active_= true;
		conn_ = std::make_shared<AMQP::Connection>(
			this,
			AMQP::Login(
				details_.username(),
				details_.password()
			),
			details_.vhost()
		);

		if(!outgoing_.empty())
			write_handler();

		self->read_handler();
	}

    virtual ~connection() { }

private:
    virtual void onData(
		AMQP::Connection *connection,
		const char *buffer,
		size_t size
	) override
	{
		auto v = std::make_shared<std::vector<char>>(
			buffer,
			buffer + size
		);
		auto self = shared_from_this();
		strand_.post([self, v] {
			self->outgoing_.insert(
				self->outgoing_.end(),
				begin(*v),
				end(*v)
			);

			/* Defer any write attempts until we're past the constructor */
			if(self->active_)
				self->write_handler();
		});
	}

    virtual void onError(
		AMQP::Connection *connection,
		const char *message
	) override
	{
		std::string m { message };
		amqp_error(m);
	}

    virtual void onConnected(AMQP::Connection *connection) override
	{
		amqp_connected(connection);
	}

	void
	write_handler()
	{
		assert(active_);
		bool writing = false;
		if(!writing_.compare_exchange_weak(writing, true))
			return;

		if(outgoing_.empty()) {
			writing_ = false;
			return;
		}

		auto self = shared_from_this();
		auto v = std::make_shared<std::vector<char>>(outgoing_.cbegin(), outgoing_.cend());
		outgoing_.clear();
		self->strand_.post([self, v]() {
			self->sock_->async_write_some(
				boost::asio::buffer(*v),
				[self, v](boost::system::error_code ec, std::size_t len) {
					self->strand_.post([self, ec, len] {
						self->writing_ = false;
						if(ec) {
							self->connection_error(ec.message());
						} else {
							self->write_handler();
						}
		//				self->strand_.post([self, ec, len]() {
		//					self->on_request_written(ec, len);
		//				});
					});
				}
			);
		});
	}

	void
	read_handler()
	{
		auto self = shared_from_this();
		// auto mb = std::make_shared<boost::asio::streambuf::mutable_buffers_type>(incoming_->prepare(4096));
		// std::cout << "about to read some" << std::endl;
		self->strand_.post([self]() {
			auto storage = std::make_shared<std::vector<char>>(4096);
			self->sock_->async_read_some(
				boost::asio::buffer(&((*storage)[0]), storage->size()),
				[self, storage](
					const boost::system::error_code &ec,
					std::size_t len
				) {
					self->strand_.post([self, ec, len, storage] {
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
					});
				}
			);
		});
	}

	/**
	 * Enable some RabbitMQ-specific features:
	 *
	 * <ul>
	 * <li>consumer_cancel_notify - ensures that we can detect situations where the consumer
	 * <li>is no longer running, and take steps to reëstablish it
	 * <li>basic.nack - NACK support for indicating that we're not interested in a message
	 * <li>publisher_confirms - ability to confirm that a message was accepted for publishing
	 * </ul>
	 */
	virtual void
	ourCapabilities(AMQP::Table &caps)
	override
	{
		caps["consumer_cancel_notify"] = true;
		caps["basic.nack"] = true;
		caps["publisher_confirms"] = true;
	}

	/**
	 * Annotate the connection with our details, for diagnostics
	 */
	virtual void
	ourProperties(AMQP::Table &props)
	override
	{
		props["version"] = "1.00";
		props["product"] = "asio-protocols";
	}

public:
	std::shared_ptr<AMQP::Channel> channel() { return std::make_shared<AMQP::Channel>( &(*conn_) ); }

	boost::signals2::signal<void(const std::string &)> amqp_error;
	boost::signals2::signal<void(AMQP::Connection *)> amqp_connected;
	boost::signals2::signal<void(const std::string &)> connection_error;
	boost::signals2::signal<void(const std::string &)> channel_error;

private:
	std::atomic<bool> active_;
	std::atomic<bool> writing_;
	std::vector<char> outgoing_;
	std::vector<char> incoming_;
	boost::asio::strand strand_;
	connection_details details_;
	std::shared_ptr<boost::asio::ip::tcp::socket> sock_;
    std::shared_ptr<AMQP::Connection> conn_;

};

};

};

