#pragma once
#define BOOST_ASIO_HAS_STD_CHRONO
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/high_resolution_timer.hpp>

namespace net {
namespace http {

class connection_pool;

class connection : virtual public std::enable_shared_from_this<connection> {
public:
	enum class transfer {
		chunked = 0,
		length = 1
	};
	connection(
		boost::asio::io_service &service,
		connection_pool &pool,
		const std::string &hostname,
		uint16_t port
	):service_(service),
	  pool_(pool),
	  hostname_(hostname),
	  port_(port),
	  closed_{ false },
	  valid_{ true },
	  already_active_{ false },
	  in_(std::make_shared<boost::asio::streambuf>()),
	  expected_bytes_{ 0 }
	{
	}

	connection() = delete;
	connection(const connection &src) = delete;
	connection(connection &&src) = delete;

	void request(
		std::function<void()> code
	)
	{
		using boost::asio::ip::tcp;
		if(already_active_) {
			// std::cerr << "Request called but we think we are currently active\n";
		}
		already_active_ = true;
		auto self = shared_from_this();
		auto resolver = std::make_shared<tcp::resolver>(service_);
		auto query = std::make_shared<tcp::resolver::query>(
			hostname_,
			std::to_string(port_)
		);
		// std::cout << "resolving " << hostname_ << ":" << std::to_string(port_) << "\n";
		resolver->async_resolve(
			*query,
			[self, query, resolver, code](
				const boost::system::error_code &ec,
				const tcp::resolver::iterator ei
			) {
				if(ec) {
					// std::cerr << "Resolve failed: " << ec.message() << "\n";
					self->close();
				} else {
					// std::cout << "Connecting\n";
					self->connect(ei)->then([self](bool) {
						return self->post_connect();
					})->on_done([code](bool) {
						code();
					});
				}
			}
		);
	}

	virtual ~connection() {
		// std::cerr << "~connection " << (void *)this << "\n";
	}

	virtual
	std::shared_ptr<
		cps::future<
			bool
		>
	>
	connect(
		const boost::asio::ip::tcp::resolver::iterator &ei
	) = 0;

	/**
	 * Attempts to write data to the underlying connection.
	 */
	virtual
	std::shared_ptr<
		cps::future<
			size_t
		>
	>
	write(std::shared_ptr<std::vector<char>> data) = 0;

	void
	write_request(std::shared_ptr<net::http::response> &&res)
	{
		auto self = shared_from_this();
		auto s = res->request().bytes();
		auto out = std::make_shared<std::vector<char>>(
			s.cbegin(), s.cend()
		);
		res_ = std::move(res);
		auto expected = out->size();
		write(out)->on_done([self, expected](const size_t) {
			self->extend_timer();
			// std::cout << "wrote " << bytes << " bytes, expected to write " << expected << " bytes\n";
		})->on_fail([self](const std::string &err) {
			// std::cerr << "Error writing: " << err << "\n";
			auto f = self->res_->completion();
			if(f->is_ready()) return;
			f->fail(err);
		});
		/**
		 * Immediately start the response handler: it's quite possible that we have an invalid
		 * request so the server could return a 400 (or any other status) before we've finished
		 * writing.
		 */
		
		// std::cout << "Streambuf size is " << in_->size() << " bytes before we start reading response\n";
	}

	virtual
	std::shared_ptr<
		cps::future<
			std::string
		>
	>
	read_delimited(const std::string &delim) = 0;
	virtual
	std::shared_ptr<
		cps::future<
			std::string
		>
	>
	read(size_t wanted) = 0;

	void handle_response()
	{
		auto self = shared_from_this();
		read_delimited("\x0D\x0A")->on_done([self](const std::string &line) {
			// std::cout << "Initial line: " << line << "\n";
			if(self->res_) {
				self->extend_timer();
				self->res_->parse_initial_line(line);
				self->read_next_header();
			} else {
				self->close();
			}
		})->on_fail([self](const std::string &err) {
			// std::cerr << "Error reading: " << err << "\n";
			if(self->res_) {
				auto f = self->res_->completion();
				if(f->is_ready()) return;
				f->fail(err);
			}
		});
	}

	void read_next_header() {
		auto self = shared_from_this();
		read_delimited("\x0D\x0A")->on_done([self](const std::string &line) {
			self->extend_timer();
			if(!line.empty()) {
				self->res_->parse_header_line(line);
				// std::cout << "header count now " << self->res_->header_count() << "\n";
				self->read_next_header();
			} else {
				try {
					self->expected_bytes_ = static_cast<size_t>(
						std::stoi(
							self->res_->header_value("Content-Length")
						)
					);
					self->transfer_mode_ = transfer::length;
					// std::cout << "Content length should be " << std::to_string(self->expected_bytes_) << " bytes\n";
					self->res_->on_header_end();
					self->read_next_body_chunk();
				} catch(const std::runtime_error &ex) {
					try {
						/* Might not have Content-Length, try TE instead */
						if(std::string::npos != self->res_->header_value("Transfer-Encoding").find("chunked")) {
							self->res_->on_header_end();
							self->transfer_mode_ = transfer::chunked;
							self->expected_bytes_ = 0;
							self->read_next_body_chunk();
						} else {
							self->close();
							if(self->res_) {
								auto f = self->res_->completion();
								if(!f->is_ready())
									f->fail("no content-length or TE");
							}
						}
					} catch(const std::runtime_error &ex) {
						self->close();
						if(self->res_) {
							auto f = self->res_->completion();
							if(!f->is_ready())
								f->fail(ex.what());
						}
					}
				}
			}
		})->on_fail([self](const std::string &err) {
			// std::cerr << "Error reading header: " << err << "\n";
			if(self->res_) {
				auto f = self->res_->completion();
				if(!f->is_ready())
					f->fail(err);
			}
		});
	}

	void read_next_body_chunk() {
		auto self = shared_from_this();
		if(transfer_mode_ == transfer::chunked) {
			read_delimited("\x0D\x0A")->on_done([self](const std::string &line) {
				self->extend_timer();
				auto chunk_size = static_cast<size_t>(
					std::stoi(
						line,
						0,
						16
					)
				);
				if(chunk_size > 0) {
					self->expected_bytes_ = chunk_size;
					self->read(chunk_size + 2)->on_done([self](const std::string &data) {
						self->res_->append_body(data.substr(0, data.size() - 2));
						self->extend_timer();
						self->read_next_body_chunk();
					})->on_fail([self](const std::string &err) {
						// std::cerr << "Error reading body data: " << err << "\n";
						if(self->res_) {
							auto f = self->res_->completion();
							if(!f->is_ready())
								f->fail(err);
						}
					});
				} else {
					self->read_delimited("\x0D\x0A")->on_done([self](const std::string &line) {
						self->extend_timer();
						self->extract_next_body_chunk("");
					});
				}
			});
		} else {
			// std::cout << "Streambuf already has " << in_->size() << " bytes, expected " << expected_bytes_ << "\n";
			read(expected_bytes_)->on_done([self](const std::string &data) {
				self->extend_timer();
				self->extract_next_body_chunk(data);
			})->on_fail([self](const std::string &err) {
				// std::cerr << "Error reading body data: " << err << "\n";
				if(self->res_) {
					auto f = self->res_->completion();
					if(!f->is_ready())
						f->fail(err);
				}
			});
		}
	}

	void extract_next_body_chunk(const std::string &data) {
		if(expected_bytes_ != data.size()) {
			// std::cerr << "Size mismatch: expected " << expected_bytes_ << ", actual content was " << data.size() << " bytes\n";
		}
		// std::cout << "all good - body is " << data.size() << " bytes\n";
		auto self = shared_from_this();
		if(transfer_mode_ == transfer::length)
			res_->body(data);
		already_active_ = false;
		auto r = res_;
		if(res_->have_header("Connection") && res_->header_value("Connection") == "close") {
			close();
		} else {
			handle_response();
			release();
		}
		// std::cout << "Marking response done\n";
		r->completion()->done(r->status_code());
		// std::cout << "Done marking response done\n";
	}

	virtual std::shared_ptr<cps::future<bool>> post_connect() {
		auto f = cps::future<bool>::create_shared();
		extend_timer();
		handle_response();
		f->done(true);
		return f;
	}

	connection_pool &pool() { return pool_; }
	virtual void remove();
	virtual void release();
	virtual void close() = 0;

	/**
	 * Refreshes the stall timer.
	 * Called after successful read/write activity.
	 */
	void
	extend_timer()
	{
		auto self = shared_from_this();
		auto target = std::chrono::milliseconds(
			static_cast<long>((res_ ? res_->stall_timeout() : 5.0f) * 1000.0f)
		);
		// std::cout << "Will wait " << target.count() << "s for " << std::to_string(res_->stall_timeout()) << "\n";
		if(!timer_) {
			timer_ = std::make_shared<boost::asio::high_resolution_timer>(
				service_
			);
		}
		timer_->expires_from_now(target);
		timer_->async_wait([self](const boost::system::error_code &ec) {
			if(ec == boost::asio::error::operation_aborted) {
				/* Timer was cancelled */
				// std::cerr << "Timer cancelled/extended\n";
			} else {
				/* Timer has expired */
				// std::cerr << "Timer expired\n";
				self->close();
				if(self->res_ && !self->res_->completion()->is_ready())
					self->res_->completion()->fail("Timeout expired");
			}
		});
	}

	/**
	 * Cancels the current stall timer.
	 * Used on disconnect.
	 */
	void
	cancel_timer()
	{
		if(timer_)
			timer_->cancel();
	}

	bool is_valid() const { return valid_ && !closed_; }

	virtual bool already_closing() {
		// std::cerr << "close() for " << (void *)this << " - " << std::boolalpha << closed_ << "\n";
		valid_ = false;
		bool is_closed = false;
		return !closed_.compare_exchange_strong(
			is_closed,
			true,
			std::memory_order_seq_cst
		);
	}

protected:
	/** The IO service */
	boost::asio::io_service &service_;
	/** Our parent connection pool */
	connection_pool &pool_;
	/** The host we'll be connecting to */
	std::string hostname_;
	/** The target port */
	uint16_t port_;
	std::atomic<bool> closed_;
	/** Flag indicating that we can be used */
	bool valid_;
	/** Flag indicating that we are already doing something */
	bool already_active_;
	/** Input buffer */
	std::shared_ptr<boost::asio::streambuf> in_;
	/** The response we're currently processing */
	std::shared_ptr<net::http::response> res_;
	/** Our stall timer */
	std::shared_ptr<boost::asio::high_resolution_timer> timer_;
	/** Chunked or content-length */
	transfer transfer_mode_;
	/** Bytes we're expecting to read */
	size_t expected_bytes_;
};

};
};

#include <net/asio/http/connection_pool.h>

namespace net {
namespace http {

inline void connection::remove() {
	pool().remove(shared_from_this());
}

inline void connection::release() {
	pool().release(shared_from_this());
}

};
};

