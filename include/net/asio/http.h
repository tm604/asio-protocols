#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <boost/regex.hpp>
#include <boost/signals2.hpp>
#include <boost/algorithm/string.hpp>

#include <cps/future.h>

#include <net/asio/http/uri.h>
#include <net/asio/http/message.h>
#include <net/asio/http/request.h>
#include <net/asio/http/response.h>
#include <net/asio/http/connection.h>
#include <net/asio/http/connection/tcp.h>
#include <net/asio/http/connection/tls.h>
#include <net/asio/http/connection_pool.h>
#include <net/asio/http/client.h>

namespace net {
namespace http {

/**
 * We may have zero or more connections to the same host information.
 * Host information is defined by any TLS-related data - client certificate,
 * for example - and host. Note that we treat each host as a separate
 * endpoint even if it happens to resolve an existing IP address: this is
 * to improve locality when using HTTP/2.
 *
 * It may be a design error for some applications. Please complain if you
 * think this is the case.
 *
 * The base connection class holds a socket - typically either plain TCP
 * or TLS-over-TCP.
 */

/**
 * The client class owns all outgoing connections and is responsible for establishing
 * new ones as necessary.
 *
 * As each request completes, a connection will be released back to the client.
 * This connection is then available to be passed out to the next request.
 */

/**
 * The server class owns all incoming connections and is responsible for accepting
 * new ones as necessary.
 */

template<typename T>
T &operator<<(T &r, const header &h)
{
	r.add_header(h);
	return r;
}

};

};

inline const net::http::uri operator"" _uri(const char *u, size_t len) { return net::http::uri(std::string { u, len }); }

