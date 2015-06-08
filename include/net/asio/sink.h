#pragma once

#include <memory>
#include <string>
#include <iostream>

#include <boost/signals2/signal.hpp>

#include <net/asio/source.h>
#include <cps/future.h>

namespace net {

/**
 * Abstract representation of an entity which is able to receive data.
 *
 */
template<typename T>
class sink:public std::enable_shared_from_this<sink<T>> {
public:
	static std::shared_ptr<sink<T>> create() { return std::make_shared<sink<T>>(); }

	void
	attach(source<T> &in) {
		auto self = this->shared_from_this();
		in.data.connect([self](const std::string &bytes) {
			// DEBUG << "Had " << bytes.size() << " bytes from source";
			auto f = self->data(bytes);
			return f;
		});
	}

	/**
	 * We've just been handed a string, use it to feed data to the buffer
	 */
#if 0
	cps::future::ptr
	write(const std::string s)
	{
		auto content = std::make_shared<std::string>(s);
		return cps::future::repeat(
			[content](cps::future::ptr in) -> bool {
				return content->empty();
			},
			[content](cps::future::ptr in) -> cps::future::ptr {
				return cps::future::create();
				// return write_chunk(*content);
			}
		);
	}

	cps::future::ptr
	write_chunk(std::string &s)
	{
		auto b = buffer();
		auto needed = s.size();
		auto available = b.writable();
		auto count = (available
		if(space > 0) {
			b.write(
				space
			);
		}
		auto code = []() {
		if(can_write()) {
			code();
		} else {
			on_write_ready()->
		}
		return content->empty();
	}
#endif

	template<typename U>
	struct combined_future {
		typedef U result_type;

		template<typename InputIterator>
		T operator()(InputIterator first, InputIterator last) const
		{
			if(first == last)
				return cps::future<bool>::create_shared()->done(0);
#if 1
			return cps::future<bool>::create_shared()->done(0);
#else
			T f = *first++;
			while(first != last) {
				f = cps::needs_all(f, *first++);
			}

			return f;
#endif
		}
	};
	boost::signals2::signal<
		std::shared_ptr<cps::future<int>>(const std::string &),
		combined_future<std::shared_ptr<cps::future<bool>>>
	> data;
};
};

