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

	/**
	 * Arbitrary HTTP request. Requires a valid method on the HTTP request instance.
	 */
	std::shared_ptr<net::http::response>
	request(net::http::request &&req)
	{
		auto endpoint = endpoint_for(req);
		auto res = std::make_shared<net::http::response>(
			std::move(req),
			stall_timeout_
		);
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

