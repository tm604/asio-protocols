#pragma once
#include <memory>
#include <string>

class response;
class uri;

class request {
public:
	virtual const uri &uri() const = 0;
	virtual const std::string &method() const = 0;
	virtual const std::string &version() const = 0;
};

class response {
public:
	/**
	 * Provide a list of zero or more valid status codes. Anything other than these
	 * will cause completion() to report failure.
	 */
	virtual std::shared_ptr<response> &expect_status() = 0;
	/** Expected content type - list of zero or more. */
	virtual std::shared_ptr<response> &expect_content_type() = 0;
	/** Expected header pair. Key, zero or more values. Failure if we don't see this header,
	 * or if it doesn't match one of the values. No values => check header is present. */
	virtual std::shared_ptr<response> &expect_header() = 0;

	/** Discards incoming body */
	virtual std::shared_ptr<response> &ignore_body() = 0;
	/** Callback for streaming */
	virtual std::shared_ptr<response> &stream_body() = 0;
	/** Returns the body, unless we discarded or streamed it */
	virtual body() = 0;

	/** Future which will complete on response end */
	virtual completion() = 0;
};

class client {
public:
	virtual std::shared_ptr<response> GET(uri u) = 0;
	virtual std::shared_ptr<response> PUT(uri u) = 0;
	virtual std::shared_ptr<response> POST(uri u) = 0;
	virtual std::shared_ptr<response> HEAD(uri u) = 0;
	virtual std::shared_ptr<response> OPTIONS(uri u) = 0;
	virtual std::shared_ptr<response> DELETE(uri u) = 0;
};

