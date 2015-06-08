#pragma once

#include <memory>
#include <string>
#include <iostream>

#include <boost/signals2/signal.hpp>

#include <cps/future.h>

namespace net {

/**
 * Abstract representation of an entity which is able to provide or generate data.
 *
 * As the counterpart to a {sink}, {source} objects can be attached to {sink}s,
 * with zero or more {sink}s allowed for each {source}. A {source} can be disconnected -
 * when this happens, any incoming datastream will be dropped.
 *
 * Incoming data is delivered to all connected {sink}s (potentially in parallel?), and
 * this data is not removed from the buffer until all {sink}s have acknowledged it.
 */
template<typename T>
class source:public std::enable_shared_from_this<source<T>> {
public:
	static std::shared_ptr<source<T>> create() { return std::make_shared<source<T>>(); }

	void reset() { }

#if 0
	template<typename T>
	struct needs_all {
		typedef T result_type;

		template<typename InputIterator>
		cps::future::ptr operator()(InputIterator first, InputIterator last) const
		{
			// If there are no slots to call, just return the
			// default-constructed value
			if(first == last ) return cps::future::create()->done();

			std::vector<cps::future::ptr> pending;
			for(auto it = first; it != last; ++it) {
				auto f = *it;
				f->on_done([f]() {
					// DEBUG << "This one's done";
				});
				pending.push_back(f);
			}
			//DEBUG << "We have " << pending.size() << " handlers in the queue";

			auto f = cps::future::needs_all(pending);
			// DEBUG << "We have " << pending.size() << " handlers in the queue";
			f->on_done([f]() {
				// DEBUG << "needs_all done";
			});
			return f;
		}

	};
#endif

	/** Called whenever there is more data to process */
	boost::signals2::signal<std::shared_ptr<cps::future<int>>(const std::string &)> data;

	/** Indicates that an error was detected, will wait for all cps::futures to complete
	 * then reset the source
	 */
	// boost::signals2::signal<cps::future::ptr(std::string)> error;
};

};

