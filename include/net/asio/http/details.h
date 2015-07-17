#pragma once
#include <string>
#include <functional>
#include <net/asio/http/uri.h>

namespace net {
namespace http {

/**
 * Represents an endpoint with distinct connection
 * characteristics.
 *
 * Details may include:
 * * Hostname, IP or vhost
 * * Port
 * * SSL certificate
 */
class details {
public:
	details(
		const net::http::uri &u
	):host_{ u.host() },
	  port_{ u.port() },
	  tls_{ u.scheme() == "https" }
	{
		// std::cout << " hd => " << string() << "\n";
	}
	details() = delete;
	virtual ~details() {
		// std::cout << "~hd\n";
	}	

	/** Our hashing function is currently based on the stringified value */
	class hash {
	public:
		std::size_t operator()(details const &hd) const {
			return std::hash<std::string>()(hd.string());
		}
	};

	/** Equality operator is, again, based on stringified value */
	class equal {
	public:
		bool operator()(details const &src, details const &dst) const {
			return src.string() == dst.string();
		}
	};

	/**
	 * Stringified value for this endpoint.
	 * Currently takes the form host:port
	 */
	std::string string() const { return (tls_ ? "https://" : "http://") + host_ + std::string { ":" } + std::to_string(port_); }

	const std::string &host() const { return host_; }
	uint16_t port() const { return port_; }
	bool tls() const { return tls_; }

private:
	std::string host_;
	uint16_t port_;
	bool tls_;
};

};
};

