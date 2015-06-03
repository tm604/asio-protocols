#pragma once

#include "stdafx.h"
#include "Log.h"

namespace net {

namespace amqp {

class connection:public AMQP::ConnectionHandler, public std::enable_shared_from_this<connection> {
private:
	bool active_;
	bool writing_;
	std::vector<char> outgoing_;
	std::vector<char> incoming_;
	boost::asio::strand strand_;
	std::shared_ptr<boost::asio::ip::tcp::socket> sock_;
    std::shared_ptr<AMQP::Connection> conn_;

    virtual void onData(
		AMQP::Connection *connection,
		const char *buffer,
		size_t size
	) override
	{
		TRACE << "AMQP has " << size << " bytes to write";
		outgoing_.insert(outgoing_.end(), buffer, buffer + size);
		TRACE << "Total outgoing now " << outgoing_.size();

		/* Defer any write attempts until we're past the constructor */
		if(active_)
			write_handler();
	}

    virtual void onError(
		AMQP::Connection *connection,
		const char *message
	) override
	{
		ERROR << "Encountered AMQP error: " << message;
	}

    virtual void onConnected(AMQP::Connection *connection) override
	{
		TRACE << "AMQP handshaking complete";
	}

	void
	write_handler()
	{
		assert(active_);
		if(writing_)
			return;
		if(outgoing_.empty())
			return;

		writing_ = true;
		TRACE << "Attempting write";
		auto self = shared_from_this();
		TRACE << "Trying to write existing data: " << outgoing_.size() << " bytes";
		auto v = std::make_shared<std::vector<char>>(outgoing_.cbegin(), outgoing_.cend());
		outgoing_.clear();
		TRACE << "Will write " << v->size() << " bytes";
		self->strand_.post([self, v]() {
			self->sock_->async_write_some(
				boost::asio::buffer(*v),
				[self, v](boost::system::error_code ec, std::size_t len) {
					TRACE << "in write callback, len = " << std::to_string(len) << ", ec = " << ec.message();
					self->writing_ = false;
					if(ec) {
						ERROR << "Had error " << ec.message();
						self->connection_error(ec.message());
					} else {
						self->write_handler();
					}
	//				self->strand_.post([self, ec, len]() {
	//					self->on_request_written(ec, len);
	//				});
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
		TRACE << "async_read_some";
		self->strand_.post([self]() {
			auto storage = std::make_shared<std::vector<char>>(4096);
			self->sock_->async_read_some(
				boost::asio::buffer(&((*storage)[0]), storage->size()),
				[self, storage](
					const boost::system::error_code &ec,
					std::size_t len
				) {
					if(ec) {
						ERROR << "AMQP network error: " << ec.message();
						self->connection_error(ec.message());
					} else {
						TRACE << "read some, len = " << std::to_string(len) << ", ec = " << ec.message();
						storage->resize(len);
						TRACE << "internal buffer was " << self->incoming_.size() << " bytes";
						self->incoming_.insert(end(self->incoming_), begin(*storage), end(*storage));
						size_t available = self->incoming_.size();
						TRACE << "total internal buffer now " << available << " bytes";
						size_t processed = 0;
						char *ptr = &((self->incoming_)[0]);
						while(available > 0) {
							TRACE << "Attempting to process " << available << " bytes from buffer";
							size_t parsed = self->conn_->parse(ptr, available);
							TRACE << "Parsed " << parsed << " bytes from buffer";
							if(parsed == 0) break;
							ptr += parsed;
							available -= parsed;
							processed += parsed;
						}
						self->incoming_.erase(begin(self->incoming_), begin(self->incoming_) + processed);
						/* Defer callback until we can read again */
						TRACE << "Once again";
						self->read_handler();
					}
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
		DEBUG << "Setting capabilities";
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
		DEBUG << "Setting properties";
		props["version"] = "1.00";
		props["product"] = "asio-protocols";
	}

public:
    connection(
		std::shared_ptr<boost::asio::ip::tcp::socket> sock,
		boost::asio::io_service &srv
	):active_{false},
	  writing_{false},
	  outgoing_{ },
	  incoming_{ },
	  strand_{ srv },
	  sock_{ sock },
	  conn_(
		std::make_shared<AMQP::Connection>(
			this,
			AMQP::Login(
				"guest",
				"guest"
			),
			"/"
		)
	  )
	{
	}

	std::shared_ptr<AMQP::Channel> channel() { return std::make_shared<AMQP::Channel>( &(*conn_) ); }

	void setup() {
		auto self = shared_from_this();
		active_= true;

		if(!outgoing_.empty())
			write_handler();

		self->read_handler();
	}

    virtual ~connection() { }

	boost::signals2::signal<void(const std::string &)> connection_error;
	boost::signals2::signal<void(const std::string &)> channel_error;
};

};

};

