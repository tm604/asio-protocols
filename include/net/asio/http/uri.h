#pragma once
#include <string>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <boost/regex.hpp>

namespace net {
namespace http {

class uri {
public:
	class query {
	public:
		query(
			const std::string &k,
			const std::string &v
		):key_{ k },
		  value_{ v }
		{
		}

		std::string encoded_string() const
		{
			return encoded_key() + "=" + encoded_value();
		}
		std::string encoded_key() const
		{
			return escape(key_);
		}
		std::string encoded_value() const
		{
			return escape(value_);
		}

		static std::string escape(const std::string &in)
		{
			std::stringstream out;
			/* FIXME Pretty sure there are better ways of doing this */
			for(auto &ch : in) {
				if(isalnum(ch) || ch == '.' || ch == '_' || ch == '~' || ch == '-')
					out << ch;
				else
					out << "%" << std::setw(2) << std::hex << static_cast<int>(ch);
			}
			return out.str();
		}

	private:
		std::string key_;
		std::string value_;
	};

	uri &operator<<(const query &q)
	{
		if(have_query()) query_ += "&";
		query_ += q.encoded_string();
		return *this;
	}

	static std::string encoded(const std::string &in)
	{
		std::stringstream out;
		/* FIXME Pretty sure there are better ways of doing this */
		for(auto &ch : in) {
			if(isalnum(ch) || ch == '.' || ch == '_' || ch == '~' || ch == '-')
				out << ch;
			else
				out << "%" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(ch);
		}
		return out.str();
	}

	static std::string decoded(const std::string &in)
	{
		std::stringstream out;
		/* FIXME Pretty sure there are better ways of doing this */
		auto it = in.cbegin();
		while(it != in.cend()) {
			if(*it == '+') {
				out << ' ';
			} else if(*it == '%') {
				++it;
				std::string s { it, it + 2 };
				out << static_cast<char>(std::stoi(s, 0, 16));
				++it;
			} else {
				out << *it;
			}
			++it;
		}
		return out.str();
	}

	/**
	 * Parses a URI from the given string.
	 *
	 * Expects input such as
	 *
	 *     "http://localhost/some/path?query=string"
	 *
	 */
	uri(
		const std::string &in
	)
	{
		static const boost::regex re(
			"^(?<scheme>[a-z]+)://(?:(?<user>[^:]*):(?<password>[^@]*)@)?(?<host>[^:/]+)(?::(?<port>\\d+))?(?<path>/[^?#]*)?(?:\\?(?<query>[^#]*))?(?:#(?<fragment>.*))?$",
			boost::regex::icase
		);
		boost::smatch matches;
		if(!regex_match(in, matches, re)) throw std::runtime_error("Failed to parse " + in);
		scheme_ = matches["scheme"];
		hostname_ = matches["host"];
		port_ = std::string(matches["port"]).empty()
		? port_for_scheme(scheme_)
		: static_cast<uint16_t>(std::stoi(matches["port"]));
		user_ = matches["user"];
		pass_ = matches["password"];
		path_ = matches["path"];
		query_ = matches["query"];
		fragment_ = matches["fragment"];
	}

	uri() = default;
	uri(const uri &src) = default;
	uri(uri &&src) = default;
	virtual ~uri() = default;

	const std::string &scheme() const { return scheme_; }
	const std::string &host() const { return hostname_; }
	const std::string &user() const { return user_; }
	const std::string &pass() const { return pass_; }
	const std::string &path() const { return path_; }
	const std::string &query_string() const { return query_; }
	const std::string &fragment() const { return fragment_; }
	bool have_query() const { return !query_.empty(); }
	uint16_t port() const { return port_; }

	/** Returns true if the port used for this URI would be the default for the scheme */
	bool is_default_port() const { return port_for_scheme(scheme_) == port_; }

	static uint16_t port_for_scheme(const std::string &scheme) {
		if(scheme == "amqp") return 5672;
		else if(scheme == "amqps") return 5671;
		else if(scheme == "http") return 80;
		else if(scheme == "https") return 443;
		else if(scheme == "imap") return 143;
		else if(scheme == "pop3") return 110;
		else if(scheme == "smtp") return 25;
		throw std::runtime_error("Unknown scheme " + scheme);
	}

	std::string string() const {
		return scheme_
		+ "://"
		+ hostname_
		+ (is_default_port() ? std::string { "" } : std::string { ":" } + std::to_string(port()))
		+ path_
		+ (have_query() ? "?" + query_ : "");
	}

private:
	std::string scheme_;
	std::string hostname_;
	std::string user_;
	std::string pass_;
	std::string path_;
	std::string query_;
	std::string fragment_;
	uint16_t port_;
};

};
};
