#pragma once
#include <string>
#include <memory>
#include <boost/signals2.hpp>

#include <cps/future.h>

#include <net/asio/http/request.h>

namespace net {
namespace http {

/**
 * A response is always associated with a request. Note that a request may
 * be "virtual" - this is the case with HTTP/2 PUSH, for example. These
 * will still have a request object.
 */
class response : public message {
public:
	response(
	):completion_(cps::future<uint16_t>::create_shared()),
	  stall_timeout_{ 30.0f }
	{
	}

	response(
		http::request &&req
	):request_(std::move(req)),
	  completion_(cps::future<uint16_t>::create_shared())
	{
	}

	response(const response &) = default;

	/**
	 * Move constructor.
	 * Note that this does not apply any existing
	 * signal handlers. I think that's probably a bug.
	 */
	response(
		response &&src
	):message(std::move(src)),
	  status_code_(std::move(src.status_code_)),
	  status_message_(std::move(src.status_message_)),
	  completion_(std::move(src.completion_))
	{
	}

	virtual ~response() {
		// std::cout << "~resp\n";
	}

	/**
	 * Parse the first line (status code and message).
	 */
	virtual void
	parse_initial_line(const std::string &in) override
	{
		size_t first = 0;
		size_t next = in.find(" ");
		if(std::string::npos == next)
			throw std::runtime_error("No response version found");
		version(in.substr(first, next - first));

		first = next + 1;
		next = in.find(" ", first);
		if(std::string::npos == next)
			throw std::runtime_error("No status code found");
		std::string sc { in.substr(first, next - first) };
		// std::cout << "Read status code string: [" << sc << "]\n";
		status_code(static_cast<uint16_t>(std::stoi(sc)));
		// std::cout << "Status code ended up as " << std::to_string(status_code()) << "\n";

		/* Assume the rest of the line is the status message */
		status_message(in.substr(next + 1));
	}

	/**
	 * Set the status code.
	 */
	void status_code(uint16_t m) {
		status_code_ = m;
		on_status_code(m);
	}

	/**
	 * Returns the current status code.
	 */
	const uint16_t &status_code() const { return status_code_; }

	/**
	 * Status message
	 */
	void status_message(const std::string &m) {
		status_message_ = m;
		on_status_message(m);
	}

	/**
	 * Returns the current status message.
	 */
	const std::string &status_message() const { return status_message_; }

	/**
	 * Returns the completion future for this response.
	 * It will resolve with the status code when done.
	 */
	std::shared_ptr<cps::future<uint16_t>>
	completion() { return completion_; }

	/**
	 * Returns the request which initiated this response.
	 */
	const http::request &request() const { return request_; }

	/**
	 * Returns the stall timeout. This is the number of seconds
	 * we'll allow to pass before marking this response as failed.
	 */
	const float stall_timeout() const { return stall_timeout_; }

public: // Signals
	boost::signals2::signal<void(uint16_t)> on_status_code;
	boost::signals2::signal<void(const std::string &)> on_status_message;

protected:
	http::request request_;
	/** e.g. 200 */
	uint16_t status_code_;
	/** e.g. 'OK' */
	std::string status_message_;
	std::shared_ptr<cps::future<uint16_t>> completion_;
	float stall_timeout_;
};

};
};

