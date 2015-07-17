#pragma once
#include <memory>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <queue>
#include <boost/asio/io_service.hpp>

#include <net/asio/http/details.h>
#include <net/asio/http/connection.h>

namespace net {
namespace http {

/**
 * A group of connections to a single endpoint.
 * This maintains zero or more connections to a target endpoint.
 * Each of the connections should be entirely interchangeable with
 * any of the others.
 * This is responsible for timing out old connections, connecting
 * where necessary, and distributing connection requests to one
 * or more TCP connections.
 */
class connection_pool {
public:
	connection_pool(
		boost::asio::io_service &service,
		const details &details
	)
	 :service_(service),
	  endpoint_(details),
	  limit_connections_{true},
	  max_connections_{32}
	{
	}

	connection_pool(const connection_pool &) = delete;
	connection_pool(connection_pool &&) = default;
	virtual ~connection_pool() = default;

	/**
	 * In order:
	 * * If we have an available connection, return it immediately
	 * * If we have not yet reached the connection limit, request a new connection and return that
	 * * Push a request onto the pending queue and return that
	 */
	std::shared_ptr<
		cps::future<
			std::shared_ptr<
				connection
			>
		>
	>
	next()
	{
		std::lock_guard<std::mutex> guard { mutex_ };

		/* Try the items in the available queue - some may have expired already */
		while(!available_.empty()) {
			// std::cout << "Have available conn, returning that\n";
			auto conn = available_.front().lock();
			available_.pop();
			if(conn)
				return cps::future<std::shared_ptr<connection>>::create_shared()->done(conn);
		}

		/* Next option: try a new connection */
		if(!limit_connections_ || connections_.size() < max_connections_) {
			// std::cout << "Can create new conn, doing so\n";
			auto conn = connect();
			connections_.push_back(conn);
			return conn;
		} else {
			/* Finally, queue the request until we have an endpoint that can deal with it */
			// std::cout << "Need to wait\n";
			auto f = cps::future<std::shared_ptr<connection>>::create_shared();
			auto start = std::chrono::high_resolution_clock::now();
			next_.push([f, start](const std::shared_ptr<connection> &conn) {
				auto elapsed = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start);
				// std::cout << "Waiting over - took " << elapsed.count() << "s\n";
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
				connection
			>
		>
	>
	connect();

	/**
	 * Release an existing connection to be used by other requests.
	 * Called by the connection when it has finished processing the current
	 * response.
	 */
	void
	release(std::shared_ptr<connection> conn)
	{
		std::function<void(std::shared_ptr<connection>)> code;
		{
			std::lock_guard<std::mutex> guard { mutex_ };
			if(next_.empty()) {
				available_.emplace(conn);
				return;
			} else {
				code = next_.front();
				next_.pop();
			}
		}
		code(conn);
	}

	/**
	 * Remove a connection entirely - usually because it has been closed
	 * by one side or the other.
	 */
	void
	remove(const std::shared_ptr<connection> &conn)
	{
		std::lock_guard<std::mutex> guard { mutex_ };
		std::cerr << "remove conn " << (void *)conn.get() << "\n";
		connections_.erase(
			std::remove_if(
				begin(connections_),
				end(connections_),
				[&conn](
					const std::shared_ptr<
						cps::future<
							std::shared_ptr<
								connection
							>
						>
					> &f
				) {
					/* Get rid of any items that are no longer of use */
					if(!f) return true;
					if(f->is_failed()) return true;
					if(f->is_cancelled()) return true;
					if(f->is_done() && f->value() == conn) return true;

					return false;
				}
			)
		);

		if(next_.empty())
			return;

		/* We've removed a connection, but we have requests in the queue,
		 * so if we're back under the limit of available connections then
		 * we may need to initiate a new connection to serve this request.
		 */
		if(!limit_connections_ || connections_.size() < max_connections_) {
			// std::cout << "Can create new conn, doing so\n";
			auto conn = connect();
			connections_.push_back(conn);
			auto self = this;
			conn->on_done([self](std::shared_ptr<connection> conn) {
				self->release(conn);
			});
			return;
		}
		std::cerr << "We have waiting connections but no slots\n";
	}

private:
	boost::asio::io_service &service_;
	details endpoint_;

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
					connection
				>
			>
		>
	> connections_;
	/** Connections that are ready to be used for requests */
	std::queue<
		std::weak_ptr<
			connection
		>
	> available_;
	/** Requests that are waiting for a connection */
	std::queue<
		std::function<
			void(std::shared_ptr<connection>)
		>
	> next_;
};

};
};

#include <net/asio/http/connection/tcp.h>
#include <net/asio/http/connection/tls.h>

namespace net {
namespace http {

inline
std::shared_ptr<
	cps::future<
		std::shared_ptr<
			connection
		>
	>
>
connection_pool::connect()
{
	// std::cout << "Connecting\n";
	std::shared_ptr<connection> conn = endpoint_.tls()
	? std::static_pointer_cast<connection>(
		std::make_shared<tls>(
			service_,
			*this,
			endpoint_.host(),
			endpoint_.port()
		)
	)
	: std::static_pointer_cast<connection>(
		std::make_shared<tcp>(
			service_,
			*this,
			endpoint_.host(),
			endpoint_.port()
		)
	);
	auto f = cps::future<std::shared_ptr<connection>>::create_shared();
	conn->request([conn, f] {
		f->done(conn);
	});
	return f;
}

};
};

