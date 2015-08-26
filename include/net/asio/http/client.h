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
		boost::asio::io_service &service
	)
	 :service_(service),
	  limit_connections_{ true },
	  max_connections_{ 8 }
	{
	}

	/**
	 * GET request.
	 */
	std::shared_ptr<net::http::response>
	GET(net::http::request &&req)
	{
		req.method("GET");
		// req.add_header(net::http::header { "Content-Length", "0" });
		auto endpoint = endpoint_for(req);
		auto res = std::make_shared<net::http::response>(
			std::move(req)
		);
		auto r = res;
		endpoint->next()->on_done([r](std::shared_ptr<connection> conn) mutable {
			// std::cout << "Have endpoint";
			conn->write_request(std::move(r));
		});
		return res;
	}

	/**
	 * POST request.
	 */
	std::shared_ptr<net::http::response>
	POST(net::http::request &&req)
	{
		req.method("POST");
		// req.add_header(net::http::header { "Content-Length", "0" });
		auto endpoint = endpoint_for(req);
		auto res = std::make_shared<net::http::response>(
			std::move(req)
		);
		auto r = res;
		endpoint->next()->on_done([r](std::shared_ptr<connection> conn) mutable {
			// std::cout << "Have endpoint";
			conn->write_request(std::move(r));
		});
		return res;
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
};

};
};

