#pragma once
#include <string>
#include <boost/regex.hpp>

#include <net/asio/http/uri.h>

namespace net {
namespace http {

/**
 * Represents a single header in a request or ressponse
 */
class header : std::pair<std::string, std::string> {
public:
	header(
		const std::string &k,
		const std::string &v,
		bool norm = true
	):std::pair<std::string, std::string>{
		norm ? normalize_key(k) : k,
		norm ? normalize_value(v) : v
	  }
	{
	}

	/** Returns this header in a canonical Key: value format. */
	std::string to_string() const { return first + ": " + second; }
	/** Returns the key for this header */
	const std::string &key() const { return first; }
	/** Returns the value for this header */
	const std::string &value() const { return second; }
	header &value(const std::string &v) { second = v; return *this; }
	std::string decoded_value() const { return uri::decoded(second); }
	std::string encoded_value() const { return uri::encoded(second); }

	/**
	 * Returns a normalised form of the key
	 */
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

	/**
	 * Normalised version of the value.
	 * TODO Apply the various rules and implement this...
	 */
	static std::string
	normalize_value(const std::string &v)
	{
		return v;
	}

	/**
	 * Returns true if the given key matches our normalised key value.
	 */
	bool matches(const std::string &k) const { return normalize_key(k) == first; }
};

};
};

