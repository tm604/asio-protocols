#pragma once

#include <memory>
#include <queue>

#include <boost/circular_buffer.hpp>
#include <boost/asio/buffer.hpp>

#include <cps/future.h>
#include "Log.h"

namespace net {

/**
 * Uses a circular_buffer underlying implementation to provide
 * size-constrained read/write.
 */
template<typename T>
class buffer:public std::enable_shared_from_this<buffer<T>> {
public:
	buffer(
		/** Number of bytes available in the initial buffer */
		size_t size
	):buffer_{std::make_shared<boost::circular_buffer<T>>(size)},
	  max_size_{size},
	  write_pending_{0}
	{
	}

	/**
	 * Returns up to 2 buffers suitable for reading. Internal state will not be changed
	 * by this function - call read_complete to mark data as being processed and ready for
	 * recycling.
	 */
	std::vector<boost::asio::const_buffer>
	read_buffers(size_t target = 0)
	{
		/* When writing data, we accumulate up to 2 buffers via array_one and array_two */
		auto bufs = std::vector<boost::asio::const_buffer>();
		if(target == 0) {
			target = max_size_;
		} else if(max_size_ < target) {
			max_size_ = target;
			buffer_->set_capacity(max_size_);
		}

		auto v = buffer_->array_one();
		if(v.second > 0) {
			size_t wanted = v.second > target ? target : v.second;
			bufs.push_back(boost::asio::buffer(v.first, wanted));
			target -= wanted;
		}
		v = buffer_->array_two();
		if(v.second > 0) {
			size_t wanted = v.second > target ? target : v.second;
			bufs.push_back(boost::asio::buffer(v.first, wanted));
		}

		/* bufs now contains enough data to satisfy the write or max_size requirement, we can async_write it */
		return bufs;
	}

	boost::asio::mutable_buffers_1
	linear_writable(size_t target = 0)
	{
		if(target == 0) {
			target = max_size_;
		} else if(max_size_ < target) {
			max_size_ = target;
			buffer_->set_capacity(max_size_);
		}
		/* Restrict buffer usage to max_size_ (== capacity()) total */
		auto diff = max_size_ - buffer_->size();
		target = diff < target ? diff : target;

		/* Ensure we have the target area allocated, since we'll be writing into it */
		auto current = buffer_->size();
		buffer_->resize(current + target);
		return boost::asio::buffer(
			buffer_->linearize() + current,
			target
		);
	}

	/**
	 * Provides a list of up to 2 buffers for writing into. The area within these buffers
	 * will be reserved but inaccessible until write_complete is called.
	 */
	std::vector<boost::asio::mutable_buffer>
	write_buffers(size_t target = 0)
	{
		auto bufs = std::vector<boost::asio::mutable_buffer>();
		if(target == 0) {
			target = max_size_;
		} else if(max_size_ < target) {
			max_size_ = target;
			buffer_->set_capacity(max_size_);
		}
		/* Restrict buffer usage to max_size_ (== capacity()) total */
		auto diff = max_size_ - buffer_->size();
		target = diff < target ? diff : target;

		/* Ensure we have the target area allocated, since we'll be writing into it */
		buffer_->resize(buffer_->size() + target);

		/* Reverse-engineer buffer positions from read buffers */
		auto a1 = buffer_->array_one();
		auto a2 = buffer_->array_two();
		DEBUG << "a1.first = " << (void *)(a1.first) << ", second=" << a1.second;
		DEBUG << "a2.first = " << (void *)(a2.first) << ", second=" << a2.second;
		auto size = a1.second + a2.second;
		auto writable = target;
		if(writable > 0) { /* Do we have anything left after the first array? */
			auto remaining = buffer_->capacity() - ((a1.first - a2.first) + a1.second);
			if(remaining > 0) {
				auto wanted = remaining > writable ? writable : remaining;
				auto ptr = (void *)(a1.first + a1.second);
				DEBUG << "Can write after first array: " << ptr << " length " << wanted;
				bufs.push_back(boost::asio::buffer(ptr, wanted));
				writable -= wanted;
			}
		}
		if(writable > 0) { /* Check for space between the second and first array */
			auto remaining = a1.first - (a2.first + a2.second);
			if(remaining > 0) {
				auto ptr = (void *)(a2.first + a2.second);
				auto wanted = remaining > writable ? writable : remaining;
				DEBUG << "Can write before first array: " << ptr << " length " << wanted;
				bufs.push_back(boost::asio::buffer(ptr, wanted));
				writable -= wanted;
			}
		}

		DEBUG << "Ended up with " << bufs.size() << " buffers in vector:";
		for(const auto &it : bufs) {
			DEBUG << " * " << boost::asio::buffer_size(it);
		}
		/* bufs now contains enough space to satisfy the read or max_size requirement, we can async_read it */
		return bufs;
	}

	/**
	 * Provides a cps::future callback for writing data.
	 */
	std::shared_ptr<cps::future<int>>
	write(
		std::function<std::shared_ptr<cps::future<int>>(std::vector<boost::asio::mutable_buffer>)> code,
		size_t size = 0
	)
	{
		auto wb = write_buffers(size);
		struct buffer_size_helper { 
			std::size_t operator()(
				std::size_t s,
				boost::asio::const_buffer const& b
			) const
			{
				return s + boost::asio::buffer_size(b); 
			}

			std::size_t operator()(
				std::size_t s,
				boost::asio::mutable_buffer const& b
			) const
			{ 
				return s + boost::asio::buffer_size(b); 
			}
		};
		auto count = std::accumulate(
			wb.begin(),
			wb.end(),
			size_t{0}, 
			buffer_size_helper()
		);
		assert((size == 0) || (count <= size));
		DEBUG << " Have total of " << count << " in buffers";

		auto f = code(wb);
		write_pending_ += count;
		pending_writes_.push(std::pair<std::shared_ptr<cps::future<int>>, size_t>(f, count));
		return f;
	}

	void push_back(T v) { buffer_->push_back(v); }

	const boost::circular_buffer<T> circular() const { return *buffer_; }

	std::pair<void *, size_t> array_one() { return buffer_->array_one(); }
	std::pair<void *, size_t> array_two() { return buffer_->array_two(); }
	void erase_begin(size_t count) { buffer_->erase_begin(count); }
	size_t size() const { return buffer_->size(); }
	size_t capacity() const { return buffer_->capacity(); }
	size_t readable() const { return buffer_->size() - write_pending_; }
	size_t writable() const { return buffer_->capacity() - buffer_->size(); }

	/** Called once a write operation completes, will mark pending futures as complete as necessary */
	void
	write_complete(size_t bytes)
	{
		/* Can't write more than we've requested */
		assert(write_pending_ >= bytes);
		auto total = bytes;
		write_pending_ -= bytes;

		/* Apply data to any pending handlers */
		while(bytes > 0 && !pending_writes_.empty()) {
			auto w = pending_writes_.front();
			if(bytes >= w.second) {
				pending_writes_.pop();
				w.first->done(total);
				bytes -= w.second;
			} else {
				w.second -= bytes;
				bytes = 0;
			}
		}

		/* We expect to have a handler for every write, so if we still have leftover bytes that's
		 * not a good thing at all
		 */
		assert(bytes == 0);
	}

private:
	std::unique_ptr<boost::circular_buffer<T>> buffer_;
	size_t max_size_;
	size_t write_pending_;
	std::queue<std::pair<std::shared_ptr<cps::future<int>>, size_t>> pending_writes_;
};

using byte_buffer = buffer<uint8_t>;

};

