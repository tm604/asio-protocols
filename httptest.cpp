#include <string>
#include <memory>
#include <mutex>
#include <iostream>
#include <unordered_map>
#include <functional>
#include <queue>
#include <boost/asio.hpp>
#include <boost/asio/read_until.hpp>

#include <net/asio/http.h>

/**
 * Represents an endpoint with distinct connection
 * characteristics.
 *
 * Details may include:
 * * Hostname, IP or vhost
 * * Port
 * * SSL certificate
 */
class http_details {
public:
	http_details(
		const net::http::uri &u
	):host_{ u.host() },
	  port_{ u.port() }
	{
		// std::cout << " hd => " << string() << "\n";
	}
	http_details() = delete;
	virtual ~http_details() {
		// std::cout << "~hd\n";
	}	

	/** Our hashing function is currently based on the stringified value */
	class hash {
	public:
		std::size_t operator()(http_details const &hd) const {
			return std::hash<std::string>()(hd.string());
		}
	};

	/** Equality operator is, again, based on stringified value */
	class equal {
	public:
		bool operator()(http_details const &src, http_details const &dst) const {
			return src.string() == dst.string();
		}
	};

	/**
	 * Stringified value for this endpoint.
	 * Currently takes the form host:port
	 */
	std::string string() const { return host_ + std::string { ":" } + std::to_string(port_); }

	const std::string &host() const { return host_; }
	uint16_t port() const { return port_; }

private:
	std::string host_;
	uint16_t port_;
};

class http_connection : public std::enable_shared_from_this<http_connection> {
public:
	http_connection(
		boost::asio::io_service &service,
		const std::string &hostname,
		uint16_t port
	):service_(service),
	  hostname_(hostname),
	  port_(port),
	  socket_(std::make_shared<boost::asio::ip::tcp::socket>(service)),
	  in_(std::make_shared<boost::asio::streambuf>()),
	  expected_bytes_{ 0 }
	{
	}

	void request(
		std::function<void()> code
	)
	{
		using boost::asio::ip::tcp;
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
					std::cerr << "Resolve failed: " << ec.message() << "\n";
				} else {
					// std::cout << "Connecting\n";
					boost::asio::async_connect(
						*(self->socket_),
						ei,
						[self, code](const boost::system::error_code &ec, const tcp::resolver::iterator &it) {
							if(ec) {
								std::cerr << "Connect failed: " << ec.message() << "\n";
							} else {
								tcp::socket::non_blocking_io nb(true);
								self->socket_->io_control(nb);
								code();
								// self->mark_ready();
							}
						}
					);
				}
			}
		);
	}

	void
	write_request(std::shared_ptr<net::http::response> &&res)
	{
		auto self = shared_from_this();
		auto out = std::make_shared<std::string>(
			res->request().bytes()
		);
		res_ = std::move(res);
		boost::asio::async_write(
			*(self->socket_),
			boost::asio::buffer(out->data(), out->size()),
			[out](const boost::system::error_code &ec, size_t bytes) {
				// std::cout << "wrote " << bytes << " bytes\n";
				if(ec) {
					std::cerr << "Error writing: " << ec.message() << "\n";
				} else {
					// std::cout << "Write complete\n";
				}
			}
		);
		self->handle_response();
	}

	void
	mark_ready()
	{
		auto self = shared_from_this();
		auto out = std::make_shared<std::string>("GET / HTTP/1.1\x0D\x0AHost: localhost\x0D\x0A\x0D\x0A");
		boost::asio::async_write(
			*(self->socket_),
			boost::asio::buffer(out->data(), out->size()),
			[out](const boost::system::error_code &ec, size_t bytes) {
				// std::cout << "wrote " << bytes << " bytes\n";
				if(ec) {
					std::cerr << "Error writing: " << ec.message() << "\n";
				} else {
					// std::cout << "Write complete\n";
				}
			}
		);
		self->handle_response();
	}

	void handle_response()
	{
		auto self = shared_from_this();
		boost::asio::async_read_until(
			*(self->socket_),
			*in_,
			"\x0D\x0A",
			[self](const boost::system::error_code &ec, size_t bytes) {
				// std::cout << "Read " << bytes << " up to newline\n";
				if(ec) {
					std::cerr << "Error reading: " << ec.message() << "\n";
				} else {
					/* Extract this first line */
					// self->in_->commit(bytes);
					std::istream is { self->in_.get() };
					std::string line { "" };
					std::copy_n(
						std::istreambuf_iterator<char>(is),
						/* Leave off the CRLF */
						bytes - 2,
						std::back_inserter(line)
					);
					is.ignore(3);
					// self->in_->consume(2);
					// std::cout << "all good - line is [" << line << "]\n";
					self->res_->parse_initial_line(line);
					// std::cout << "Version: " << self->res_->version() << "\n";
					// std::cout << "Code:    " << self->res_->status_code() << "\n";
					// std::cout << "Message: " << self->res_->status_message() << "\n";
					self->read_next_header();
				}
			}
		);
	}

	void read_next_header() {
		auto self = shared_from_this();
		boost::asio::async_read_until(
			*(self->socket_),
			*in_,
			"\x0D\x0A",
			[self](const boost::system::error_code &ec, size_t bytes) {
				// std::cout << "Read " << bytes << " up to newline\n";
				if(ec) {
					std::cerr << "Error reading header: " << ec.message() << "\n";
				} else if(bytes > 2) {
					/* Extract this line */
					self->in_->commit(bytes);
					std::istream is { self->in_.get() };
					std::string line { "" };
					std::copy_n(
						std::istreambuf_iterator<char>(is),
						/* Leave off the CRLF */
						bytes - 2,
						std::back_inserter(line)
					);
					is.ignore(3);
					// self->in_->consume(2);
					// std::cout << "all good - line is [" << line << "]\n";
					self->res_->parse_header_line(line);
					// std::cout << "header count now " << self->res_->header_count() << "\n";
					self->read_next_header();
				} else {
					self->in_->commit(bytes);
					std::istream is { self->in_.get() };
					is.ignore(2);
					/* End of headers */
					// std::cout << "Finished headers\n";
					self->expected_bytes_ = static_cast<size_t>(std::stoi(self->res_->header_value("Content-Length")));
					// std::cout << "Content length should be " << std::to_string(self->expected_bytes_) << " bytes\n";
					self->res_->on_header_end();
					self->read_next_body_chunk();
				}
			}
		);
	}

	void read_next_body_chunk() {
		auto self = shared_from_this();
		// std::cout << "Streambuf already has " << in_->size() << " bytes\n";
		if(in_->size() < expected_bytes_) {
			boost::asio::async_read(
				*(self->socket_),
				*in_,
				boost::asio::transfer_exactly(expected_bytes_ - in_->size()),
				[self](const boost::system::error_code &ec, size_t bytes) {
					// std::cout << "Read " << bytes << " for body\n";
					if(ec) {
						std::cerr << "Error reading body chunk: " << ec.message() << "\n";
					} else {
						self->extract_next_body_chunk();
					}
				}
			);
		} else {
			self->extract_next_body_chunk();
		}
	}

	void extract_next_body_chunk() {
		if(in_->size() >= expected_bytes_) {
			in_->commit(expected_bytes_);
			std::istream is { in_.get() };
			std::string line { "" };
			std::copy_n(
				std::istreambuf_iterator<char>(is),
				expected_bytes_,
				std::back_inserter(line)
			);
			if(expected_bytes_ != line.size()) {
				std::cerr << "Size mismatch: expected " << expected_bytes_ << ", actual content was " << line.size() << " bytes\n";
			}
			// std::cout << "all good - body is " << line.size() << " bytes\n";
			auto self = shared_from_this();
			res_->body(line);
			res_->completion()->done(res_);
		} else {
			in_->commit(expected_bytes_);
			std::istream is { in_.get() };
			std::string line { "" };
			std::copy_n(
				std::istreambuf_iterator<char>(is),
				/* Leave off the CRLF */
				expected_bytes_,
				std::back_inserter(line)
			);
			// std::cout << "partial data - have " << line.size() << " bytes\n";
			read_next_body_chunk();
		}
	}

private:
	boost::asio::io_service &service_;
	std::string hostname_;
	uint16_t port_;
	std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
	std::shared_ptr<boost::asio::streambuf> in_;
	std::shared_ptr<net::http::response> res_;
	size_t expected_bytes_;
};

/**
 * A group of connections to a single endpoint.
 * This maintains zero or more connections to a target endpoint.
 * Each of the connections should be entirely interchangeable with
 * any of the others.
 * This is responsible for timing out old connections, connecting
 * where necessary, and distributing connection requests to one
 * or more TCP connections.
 */
class http_connection_pool {
public:
	http_connection_pool(
		boost::asio::io_service &service,
		const http_details &details
	)
	 :service_(service),
	  endpoint_(details),
	  limit_connections_{true},
	  max_connections_{8}
	{
	}
	http_connection_pool(const http_connection_pool &) = delete;
	http_connection_pool(http_connection_pool &&) = default;
	virtual ~http_connection_pool() = default;

	/**
	 * In order:
	 * * If we have an available connection, return it immediately
	 * * If we have not yet reached the connection limit, request a new connection and return that
	 * * Push a request onto the pending queue and return that
	 */
	std::shared_ptr<
		cps::future<
			std::shared_ptr<
				http_connection
			>
		>
	>
	next()
	{
		std::lock_guard<std::mutex> guard { mutex_ };
		if(!available_.empty()) {
			//std::cout << "Have available conn, returning that\n";
			auto conn = available_.front();
			available_.pop();
			return cps::future<std::shared_ptr<http_connection>>::create_shared()->done(conn);
		} else if(!limit_connections_ || connections_.size() < max_connections_) {
			// std::cout << "Can create new conn, doing so\n";
			auto conn = connect();
			connections_.push_back(conn);
			return conn;
		} else {
			// std::cout << "Need to wait\n";
			auto f = cps::future<std::shared_ptr<http_connection>>::create_shared();
			next_.push([f](const std::shared_ptr<http_connection> &conn) {
				f->done(conn);
			});
			return f;
		}
	}

	/**
	 * Establish a new connection.
	 */
	std::shared_ptr<
		cps::future<
			std::shared_ptr<
				http_connection
			>
		>
	>
	connect()
	{
		// std::cout << "Connecting\n";
		auto conn = std::make_shared<http_connection>(
			service_,
			endpoint_.host(),
			endpoint_.port()
		);
		auto f = cps::future<std::shared_ptr<http_connection>>::create_shared();
		conn->request([conn, f]() {
			f->done(conn);
		});
		return f;
	}

	void
	release(std::shared_ptr<http_connection> conn)
	{
		std::function<void(std::shared_ptr<http_connection>)> code;
		{
			std::lock_guard<std::mutex> guard { mutex_ };
			if(next_.empty()) {
				available_.push(conn);
				return;
			} else {
				code = next_.front();
				next_.pop();
			}
		}
		code(conn);
	}

private:
	boost::asio::io_service &service_;
	http_details endpoint_;

	std::mutex mutex_;
	/** If true, we limit the number of connections we allow to our endpoint */
	bool limit_connections_;
	/** If limit_connections_ is set, this defines the number of connections we'll allow */
	size_t max_connections_;
	/** All connections, whether in use or not */
	std::vector<
		std::shared_ptr<
			cps::future<
				std::shared_ptr<
					http_connection
				>
			>
		>
	> connections_;
	/** Connections that are ready to be used for requests */
	std::queue<
		std::shared_ptr<
			http_connection
		>
	> available_;
	/** Requests that are waiting for a connection */
	std::queue<
		std::function<
			void(std::shared_ptr<http_connection>)
		>
	> next_;
};

class hclient {
public:
	hclient(
		boost::asio::io_service &service
	)
	 :service_(service)
	{
	}

	std::shared_ptr<net::http::response>
	GET(net::http::request &&req)
	{
		req.method("GET");
		// req.add_header(net::http::header { "Content-Length", "0" });
		auto endpoint = endpoint_for(req);
		auto res = std::make_shared<net::http::response>(
			std::move(req)
		);
		endpoint->next()->on_done([res](std::shared_ptr<http_connection> conn) mutable {
			// std::cout << "Have endpoint";
			conn->write_request(std::move(res));
		});
		return res;
	}

	std::shared_ptr<http_connection_pool>
	endpoint_for(const net::http::request &req)
	{
		std::lock_guard<std::mutex> guard { mutex_ };
		auto details = details_for(req);
		if(endpoints_.count(details) == 0) {
			// std::cout << "Create new pool\n";
			auto pool = std::make_shared<http_connection_pool>(
				service_,
				details
			);
			endpoints_.emplace(
				std::make_pair(
					details,
					pool
				)
			);
			return pool;
		} else {
			// std::cout << "Use existing pool\n";
		}
		return endpoints_.at(details);
	}

	static
	http_details
	details_for(const net::http::request &req)
	{
		return http_details(req.uri());
	}

private:
	boost::asio::io_service &service_;
	std::mutex mutex_;
	/** Represents all connection pools */
	std::unordered_map<
		// std::reference_wrapper<
			http_details, // const
		// >,
		std::shared_ptr<http_connection_pool>,
		http_details::hash,
		http_details::equal
	> endpoints_;
};

int
main(int argc, const char *argv[])
{
	boost::asio::io_service service { };
	auto client_ = std::make_shared<hclient>(service);
	bool show_request = false;
	bool show_headers = false;
	for(int i = 1; i < argc; ++i) {
		auto v = std::string { argv[i] };
		if(v == "--headers") {
			show_headers = true;
		} else if(v == "--request") {
			show_request = true;
		} else {
			auto req = net::http::request {
				net::http::uri {
					argv[i]
				}
			};
			req << net::http::header("User-agent", "some-user-agent");
			auto res = client_->GET(
				std::move(req)
			);
			res->completion()->on_done([i, show_request, show_headers](const std::shared_ptr<net::http::response> &res) {
				if(show_request) {
					std::cout << res->request().bytes() << "\n";
				}
				if(show_headers) {
					res->each_header([](const net::http::header &h) {
						std::cout << h.key() << ": " << h.value() << "\n";
					});
					std::cout << "\n";
				}
				std::cout << res->body();
			});
		}
	}

	// std::cout << "run\n";
	service.run();
	// std::cout << "done\n";
}
