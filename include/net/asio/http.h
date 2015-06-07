#pragma once
#include <string>
#include <vector>
#include <boost/regex.hpp>
#include <boost/signals2.hpp>

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
 */
class connection { };
class client { };

/** Represents a single header in a request or ressponse */
class header : std::pair<std::string, std::string> {
public:
	header(
		const std::string &k,
		const std::string &v
	):std::pair<std::string, std::string>{
		normalize_key(k),
		normalize_value(v)
	  }
	{
	}

	std::string to_string() const { return first + ": " + second; }
	const std::string &key() const { return first; }
	const std::string &value() const { return second; }

	static std::string
	normalize_key(const std::string &k)
	{
		// s/(?:^|-)([^-]*)/\U$1\L$2/g
		return boost::regex_replace(
			k,
			boost::regex("(?:^|-)\\K([^-]+)"),
			"\\L\\u\\1",
			boost::match_default | boost::format_all
		);
	}

	static std::string
	normalize_value(const std::string &v)
	{
		return v;
	}

	bool matches(const std::string &k) const { return normalize_key(k) == first; }
};

/**
 * Standard GET/HEAD/POST/PUT/etc. request.
 *
 * As an HTTP/1.1 sink, this will pull HTTP/1.1-formatted content and populate
 * the request structures.
 *
 * As an HTTP/1.1 source, the request generates bytes similar to the following:
 * <code>
 * GET / HTTP/1.1
 * Host: example.com
 * User-Agent: something
 * </code>
 */
class request {
public:

	void method(const std::string &m) {
		method_ = m;
		on_method(m);
	}
	const std::string &method(const std::string &m) const { return method_; }

	void version(const std::string &m) {
		version_ = m;
		on_version(m);
	}
	const std::string &version(const std::string &m) const { return version_; }

	void request_path(const std::string &m) {
		request_path_ = m;
		on_request_path(m);
	}
	const std::string &request_path(const std::string &m) const { return request_path_; }

	size_t header_count() const { return headers_.size(); }

	const std::string &header_value(const std::string &k)
	{
		for(auto &h : headers_)
			if(h.matches(k)) return h.value();

		throw std::runtime_error("header " + k + " not found");
	}

	request &
	operator<<(const header &h) {
		headers_.push_back(h);
		on_header_added(h);
		return *this;
	}

public: // Signals
	boost::signals2::signal<void(const header &)> on_header_added;
	boost::signals2::signal<void(const header &)> on_header_removed;
	boost::signals2::signal<void(const std::string &)> on_method;
	boost::signals2::signal<void(const std::string &)> on_version;
	boost::signals2::signal<void(const std::string &)> on_request_path;

protected:
	/** e.g. 'GET', 'POST' */
	std::string method_;
	/** Typically 'HTTP/1.1' */
	std::string version_;
	/** Full path info from the first line, may be a complete URI */
	std::string request_path_;
	std::vector<header> headers_;
};

/**
 * A response is always associated with a request. Note that a request may
 * be "virtual" - this is the case with HTTP/2 PUSH, for example. These
 * will still have a request object.
 */
class response {
	/** Typically 'HTTP/1.1' */
	std::string version_;
	/** e.g. 200 */
	uint16_t status_code_;
	/** e.g. 'OK' */
	std::string status_message_;
};

};

};

