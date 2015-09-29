#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <boost/asio.hpp>

#include <net/asio/http/response.h>
#include <net/asio/http/connection_pool.h>

namespace net {
namespace http {

template<typename T>
struct chained
{
	typedef T result_type;

	template<typename InputIterator>
	T operator()(
		InputIterator first,
		InputIterator last
	) const
	{
		while(first != last) {
			if(!*first) return false;
			++first;
		}

		return true;
	}
};

class client {
public:
	client(
		boost::asio::io_service &service,
		float stall_timeout = 30.0f
	)
	 :service_(service),
	  limit_connections_{ true },
	  max_connections_{ 8 },
	  stall_timeout_{ stall_timeout }
	{
	}

	std::function<void(const cps::future<uint16_t> &)>
	completion_handler(
		std::shared_ptr<connection_pool> endpoint,
		std::shared_ptr<net::http::response> res,
		int retry
	)
	{
		auto self = this;
		return [self, endpoint, res, retry](const cps::future<uint16_t> &f) {
			/* Our response has either been delivered, or we had a failure.
			 * Delegate to existing handlers first.
			 */
			auto v = self->on_completion(f, res, retry);
			if(!v) {
				/* Something didn't like the response and wants us to retry */
				res->reset();
				res->current_completion()->on_ready(self->completion_handler(endpoint, res, retry + 1));
				endpoint->next()->on_done([res](std::shared_ptr<connection> conn) {
					// std::cout << "Have endpoint";
					conn->write_request(res);
				});
			} else {
				if(f.is_done())
					res->completion()->done(f.value());
				else if(f.is_failed())
					res->completion()->fail_from(f);
				else if(f.is_cancelled())
					res->completion()->cancel();
			}
		};
	}

	/**
	 * Arbitrary HTTP request. Requires a valid method on the HTTP request instance.
	 */
	std::shared_ptr<net::http::response>
	request(net::http::request &&req)
	{
		auto self = this;
		auto endpoint = endpoint_for(req);
		auto res = std::make_shared<net::http::response>(
			std::move(req),
			stall_timeout_
		);

		res->current_completion()->on_ready(completion_handler(endpoint, res, 0));

		endpoint->next()->on_done([res](std::shared_ptr<connection> conn) {
			// std::cout << "Have endpoint";
			conn->write_request(res);
		});
		return res;
	}

	/**
	 * GET request.
	 */
	std::shared_ptr<net::http::response>
	GET(net::http::request &&req)
	{
		req.method("GET");
		return request(std::move(req));
	}

	/**
	 * POST request.
	 */
	std::shared_ptr<net::http::response>
	POST(net::http::request &&req)
	{
		req.method("POST");
		return request(std::move(req));
	}

	/**
	 * POST request.
	 */
	std::shared_ptr<net::http::response>
	PUT(net::http::request &&req)
	{
		req.method("PUT");
		return request(std::move(req));
	}

	/**
	 * HEAD request.
	 */
	std::shared_ptr<net::http::response>
	HEAD(net::http::request &&req)
	{
		req.method("HEAD");
		return request(std::move(req));
	}

	/**
	 * OPTIONS request.
	 */
	std::shared_ptr<net::http::response>
	OPTIONS(net::http::request &&req)
	{
		req.method("OPTIONS");
		return request(std::move(req));
	}

	/**
	 * Returns the connection pool for the given request.
	 */
	std::shared_ptr<connection_pool>
	endpoint_for(const net::http::request &req)
	{
		std::lock_guard<std::mutex> guard { mutex_ };
		auto details = details_for(req);
		if(endpoints_.count(details) == 0) {
			// std::cout << "Create new pool\n";
			auto pool = std::make_shared<connection_pool>(
				service_,
				details
			);
			pool->max_connections(max_connections_);
			pool->limit_connections(limit_connections_);
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

	/**
	 * Returns the details object for the given request.
	 */
	static
	details
	details_for(const net::http::request &req)
	{
		return details(req.uri());
	}

	virtual void
	max_connections(size_t n)
	{
		std::lock_guard<std::mutex> guard { mutex_ };
		max_connections_ = n;
		for(auto &entry : endpoints_) {
			entry.second->max_connections(n);
		}
	}

	virtual void
	limit_connections(bool limit)
	{
		std::lock_guard<std::mutex> guard { mutex_ };
		limit_connections_ = limit;
		for(auto &entry : endpoints_) {
			entry.second->limit_connections(limit);
		}
	}

	virtual void
	stall_timeout(float sec)
	{
		stall_timeout_ = sec;
	}

public:
	/** Supports chained handlers for completion events */
	boost::signals2::signal<
		bool(const cps::future<uint16_t> &, std::shared_ptr<net::http::response>, int),
		chained<bool>
	> on_completion;

private:
	boost::asio::io_service &service_;
	std::mutex mutex_;
	bool limit_connections_;
	size_t max_connections_;
	/** Represents all connection pools */
	std::unordered_map<
		// std::reference_wrapper<
			details, // const
		// >,
		std::shared_ptr<connection_pool>,
		details::hash,
		details::equal
	> endpoints_;
	float stall_timeout_;
};

};
};

