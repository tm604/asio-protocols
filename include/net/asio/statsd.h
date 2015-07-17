#pragma once
#include <memory>
#include <string>

#include <cps/future.h>

namespace net {
namespace statsd {

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

	std::shared_ptr<cps::future<std::shared_ptr<net::statsd::connection>>>
	connect(
		const connection_details &cd
	)
	{
		using boost::asio::ip::udp;
		auto self = shared_from_this();
		auto f = cps::future<std::shared_ptr<net::statsd::connection>>::create_shared();

		try {
			auto resolver = std::make_shared<udp::resolver>(service_);
			auto query = std::make_shared<udp::resolver::query>(
				cd.host(),
				std::to_string(cd.port())
			);
			auto socket_ = std::make_shared<udp::socket>(service_);
			socket_->open(udp::v4());

			resolver->async_resolve(
				*query,
				[query, socket_, resolver, f, cd, self](boost::system::error_code ec, udp::resolver::iterator ei) {
					if(ec) {
						f->fail(ec.message());
					} else {
						udp::socket::non_blocking_io nb(true);
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
	void timing(const std::string &k, float v) { send(k, std::to_string(static_cast<uint64_t>(1000.0f * v)) + "|ms"); }
	void gauge(const std::string &k, int64_t v) { send(k, std::to_string(v) + "|g"); }
	void delta(const std::string &k, int64_t v) { send(k, std::to_string(v) + "|c"); }
	void inc(const std::string &k) { delta(k, 1); }
	void dec(const std::string &k) { delta(k, -1); }

protected:
	cps::future<int>
	send(const std::string &k, std::string v)
	{
		size_t len = k.size() + v.size() + 1;
		auto data = std::unique_ptr<std::vector<uint8_t>>(new std::vector<uint8_t>(len));
		std::copy(begin(k), end(k), back_inserter(data));
		data.emplace_back(':');
		std::copy(begin(v), end(v), back_inserter(data));
		assert(data.size() == len);
		auto code = std::bind( // workaround for lack of C++14 [x=y] init capture
			[len](
				const std::unique_ptr<std::vector<uint8_t>> &data,
				boost::system::error_code ec,
				std::size_t bytes_sent
			) {
				if(ec) {
				// error
				}
				if(bytes_sent != len) {
				// ??
				}
			},
			std::move(data),
			_1,
			_2
		);
		socket_->async_send(
			boost::asio::buffer(*data, len),
			target_,
			code
		);
	}

private:
	boost::asio::io_service &service_;
};

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

};
};

