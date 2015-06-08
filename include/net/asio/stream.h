#pragma once
#include <boost/signals2.hpp>

#include <net/asio/source.h>
#include <net/asio/sink.h>

namespace net {

/**
 * A stream represents a TCP connection.
 */
class stream {
public:
	stream() = default;
	stream(const stream &) = default;

#if 0
	stream(
		stream &&src
	):on_stall_timeout_change(std::move(src.on_stall_timeout_change)),
	  on_minimum_bandwidth_change(std::move(src.on_minimum_bandwidth_change)),
	  on_maximum_bandwidth_change(std::move(src.on_maximum_bandwidth_change)),
	  stall_timeout_(std::move(src.stall_timeout_)),
	  minimum_bandwidth_(std::move(src.minimum_bandwidth_)),
	  maximum_bandwidth_(std::move(src.maximum_bandwidth_)),
	  src_(std::move(src.src_)),
	  sink_(std::move(src.sink_)),
	{
	}
#endif

	virtual ~stream() = default;

	void stall_timeout(size_t ms) { auto old = stall_timeout_; stall_timeout_ = ms; on_stall_timeout_change(*this, ms, old); }
	size_t stall_timeout() const { return stall_timeout_; }
	void minimum_bandwidth(size_t mb) { auto old = minimum_bandwidth_; minimum_bandwidth_ = mb; on_minimum_bandwidth_change(*this, mb, old); }
	size_t minimum_bandwidth() const { return minimum_bandwidth_; }
	void maximum_bandwidth(size_t mb) { auto old = maximum_bandwidth_; maximum_bandwidth_ = mb; on_maximum_bandwidth_change(*this, mb, old); }
	size_t maximum_bandwidth() const { return maximum_bandwidth_; }

// Signals
	boost::signals2::signal<void(const stream &, size_t, size_t)> on_stall_timeout_change;
	boost::signals2::signal<void(const stream &, size_t, size_t)> on_minimum_bandwidth_change;
	boost::signals2::signal<void(const stream &, size_t, size_t)> on_maximum_bandwidth_change;
	boost::signals2::signal<void(const stream &)> under_minimum_bandwidth;
	boost::signals2::signal<void(const stream &)> over_maximum_bandwidth;

protected:
	size_t stall_timeout_;
	size_t minimum_bandwidth_;
	size_t maximum_bandwidth_;

	source<uint8_t> src_;
	sink<uint8_t> sink_;
};

};

