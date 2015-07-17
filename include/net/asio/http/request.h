#pragma once
#include <string>

#include <net/asio/http/uri.h>
#include <net/asio/http/message.h>

namespace net {
namespace http {

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
class request : public message {
public:
	request() = default;
	request(const request &) = default;
	request(
		const uri &u
	)
	/**
	 * FIXME Actual path varies depending on request type and destination:
	 * CONNECT example.com:80
	 * GET /some/host
	 * GET http://other.host.com/path
	 * OPTIONS *
	 */
	 :message("HTTP/1.1"),
	  uri_(u),
	  request_path_(u.path())
	{
		if(request_path_.empty()) request_path_ = "/";
		if(u.have_query()) request_path_ += std::string { "?" } + u.query_string();
		add_header(header { "Host", u.host() });
	}

	/**
	 * Move constructor.
	 * Note that this does not apply any existing
	 * signal handlers. I think that's probably a bug.
	 */
	request(
		request &&src
	):message(std::move(src)),
	  method_(std::move(src.method_)),
	  request_path_(std::move(src.request_path_))
	{
	}

	virtual ~request() = default;

	virtual void
	parse_initial_line(const std::string &in) override
	{
		size_t first = 0;
		size_t next = in.find(" ");
		if(std::string::npos == next)
			throw std::runtime_error("No request method found");
		method(in.substr(first, next - first));

		first = next + 1;
		next = in.find(" ", first);
		if(std::string::npos == next)
			throw std::runtime_error("No request path found");
		request_path(in.substr(first, next - first));

		/* Assume the rest of the line is a version.
		 * We don't apply any checks at this point,
		 * although technically we really should at
		 * least look for 'HTTP/something' making
		 * this accept anything may be useful for non-HTTP
		 * protocols in future.
		 */
		version(in.substr(next + 1));
	}

	/**
	 * Sets the HTTP request method.
	 */
	void method(const std::string &m) {
		method_ = m;
		on_method(m);
	}

	/**
	 * Returns the current HTTP request method.
	 */
	const std::string &method() const { return method_; }

	const http::uri &uri() const { return uri_; }

	void request_path(const std::string &m) {
		request_path_ = m;
		on_request_path(m);
	}
	const std::string &request_path() const { return request_path_; }

	request &authorisation(
		const std::string &type,
		const std::string &details
	) {
		add_header(header("Authorization", type + " " + details));
		return *this;
	}

	virtual std::string
	bytes() const
	{
		std::stringstream ss;
		ss << method() << " " << request_path() << " " << version() << "\x0D\x0A";
		each_header([&ss](const header &h) {
			ss << h.key() << ": " << h.value() << "\x0D\x0A";
		});
		ss << "\x0D\x0A";
		ss << body();
		return ss.str();
	}

public: // Signals
	boost::signals2::signal<void(const std::string &)> on_method;
	boost::signals2::signal<void(const std::string &)> on_request_path;

protected:
	class uri uri_;
	/** e.g. 'GET', 'POST' */
	std::string method_;
	/** Full path info from the first line, may be a complete URI */
	std::string request_path_;
};

};
};

