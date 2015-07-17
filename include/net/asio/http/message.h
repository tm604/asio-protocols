#pragma once
#include <string>
#include <vector>
#include <boost/signals2.hpp>

#include <net/asio/http/header.h>

namespace net {
namespace http {

/**
 * Base class used by HTTP requests and responses.
 */
class message {
public:
	message() = default;
	message(
		const std::string &v
	):version_(v)
	{
	}

	message(const message &) = default;
	virtual ~message() = default;

	/**
	 * Move constructor.
	 * Note that this does not apply any existing
	 * signal handlers. I think that's probably a bug.
	 */
	message(
		message &&src
	):version_(std::move(src.version_)),
	  headers_(std::move(src.headers_)),
	  body_(std::move(src.body_))
	{
	}

	void
	parse_data(const std::string &in)
	{
		/** FIXME make this constexpr + char* ? */
		const std::string line_sep = "\x0D\x0A";

		auto end = in.find(line_sep);
		if(end == std::string::npos)
			throw std::runtime_error("Invalid initial line");

		/* We have the first line, extract method/path/version */
		parse_initial_line(in.substr(0, end));

		size_t start;
		while(true) {
			start = end + line_sep.size();
			end = in.find(line_sep, start);
			if(end == std::string::npos)
				throw std::runtime_error("Invalid data while parsing headers");
			if(start == end) {
				on_header_end();
				break;
			} else {
				parse_header_line(in.substr(start, end - start));
			}
		}

		/* Delegate to body handler - T-E and Content-Length
		 * are mostly responsible for determining actual body
		 * content/presence here.
		 */
		parse_body(in.substr(start + line_sep.size()));
	}

	virtual void parse_initial_line(const std::string &in) = 0;

	virtual void
	parse_header_line(const std::string &in)
	{
		if(in.empty()) {
			on_header_end();
			return;
		}

		size_t next = in.find_first_of(":");
		if(std::string::npos == next)
			throw std::runtime_error("No header name found");

		auto k = in.substr(0, next);
		auto v = in.substr(next + 1);
		boost::algorithm::trim(v);
		add_header(header { k, v });
	}

	virtual void
	parse_body(const std::string &)
	{
	}

	virtual void version(const std::string &m) {
		version_ = m;
		on_version(m);
	}

	const std::string &version() const { return version_; }

	size_t header_count() const { return headers_.size(); }
	bool have_header(const std::string &k) const {
		for(auto &h : headers_)
			if(h.matches(k)) return true;
		return false;
	}

	const std::string &header_value(const std::string &k)
	{
		for(auto &h : headers_)
			if(h.matches(k)) return h.value();

		throw std::runtime_error("header " + k + " not found");
	}

	virtual message &add_header(
		const header &h
	) {
		headers_.push_back(h);
		on_header_added(h);
		return *this;
	}

	virtual message &set_header(
		const std::string &k,
		const std::string &v
	) {
		for(auto &h : headers_) {
			if(h.matches(k)) {
				h.value(v);
				return *this;
			}
		}
		return add_header(header(k, v));
	}

	virtual std::string content_type(
	) {
		for(auto &h : headers_) {
			if(h.matches("Content-Type")) {
				auto type = h.value();
				auto separator = type.find(";");
				if(std::string::npos == separator)
					return type;
				return type.substr(0, separator);
			}
		}
		throw std::runtime_error("no content-type");
	}

	virtual void each_header(
		std::function<void(const header &)> code
	) const {
		for(auto &it : headers_) {
			code(it);
		}
	}

	virtual const std::string &body() const { return body_; }
	virtual void body(const std::string &in) {
		body_ = in;
		set_header("Content-Length", std::to_string(body_.size()));
	}

// Signals
	boost::signals2::signal<void(const header &)> on_header_added;
	boost::signals2::signal<void(const header &)> on_header_removed;
	boost::signals2::signal<void(const std::string &)> on_version;
	boost::signals2::signal<void()> on_header_end;

protected:
	/** Typically 'HTTP/1.1' */
	std::string version_;
	std::vector<header> headers_;
	std::string body_;
};

};
};

