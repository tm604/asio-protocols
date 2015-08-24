#pragma once
#include <memory>
#include <string>

#include <cps/future.h>

#include <boost/asio.hpp>

namespace net {
namespace statsd {

class connection_details {
public:
	connection_details(
		const std::string &host = "localhost",
		const uint16_t port = 8125
	):host_{ host },
	  port_{ port }
	{
	}

	uint16_t default_port() const { return 8125; }

	std::string host() const { return host_; }
	uint16_t port() const { return port_; }

private:
	std::string host_;
	uint16_t port_;
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

	std::shared_ptr<cps::future<int>>
	connect(
		const connection_details &cd
	)
	{
		using boost::asio::ip::udp;
		auto self = shared_from_this();
		auto f = cps::future<int>::create_shared();

		try {
			auto resolver = std::make_shared<udp::resolver>(service_);
			auto query = std::make_shared<udp::resolver::query>(
				cd.host(),
				std::to_string(cd.port())
			);
			socket_ = std::make_shared<udp::socket>(service_);
			socket_->open(udp::v4());

			resolver->async_resolve(
				*query,
				[query, resolver, f, cd, self](boost::system::error_code ec, udp::resolver::iterator ei) {
					if(ec) {
						f->fail(ec.message());
					} else {
						udp::socket::non_blocking_io nb(true);
						self->target_ = *ei;
						f->done(0);
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

	/**
	 * Records timing information for the given key.
	 * @param k 
	 */
	std::shared_ptr<cps::future<int>> timing(const std::string &k, float v) { return send(k, std::to_string(static_cast<uint64_t>(1000.0f * v)) + "|ms"); }
	std::shared_ptr<cps::future<int>> gauge(const std::string &k, int64_t v) { return send(k, std::to_string(v) + "|g"); }
	std::shared_ptr<cps::future<int>> delta(const std::string &k, int64_t v) { return send(k, std::to_string(v) + "|c"); }
	std::shared_ptr<cps::future<int>> inc(const std::string &k) { return delta(k, 1); }
	std::shared_ptr<cps::future<int>> dec(const std::string &k) { return delta(k, -1); }

protected:
	std::shared_ptr<cps::future<int>>
	send(const std::string &k, std::string v)
	{
		size_t len = k.size() + v.size() + 1;
		auto data = std::make_shared<std::vector<char>>();
		std::copy(begin(k), end(k), back_inserter(*data));
		data->emplace_back(':');
		std::copy(begin(v), end(v), back_inserter(*data));
		assert(data->size() == len);
		auto f = cps::future<int>::create_shared();
		socket_->async_send_to(
			boost::asio::buffer(*data, len),
			target_,
			[f, len, data](
				boost::system::error_code ec,
				std::size_t bytes_sent
			) {
				if(ec) {
					f->fail(ec.message());
				} else if(bytes_sent != len) {
					f->fail("invalid length");
				} else {
					f->done(1);
				}
			}
		);
		return f;
	}

private:
	boost::asio::io_service &service_;
	std::shared_ptr<boost::asio::ip::udp::socket> socket_;
	boost::asio::ip::udp::endpoint target_;
};

#if 0
class key {
public:
	void
	operator=(size_t v) {
		statsd_.gauge(key_, v);
	}

	void
	operator++() {
		statsd_.inc(key_);
	}

	void
	operator--() {
		statsd_.inc(key_);
	}

private:
	statsd &statsd_;
	std::string key_;
};
#endif

};
};

