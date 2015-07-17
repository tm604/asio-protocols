/**
 * @file streams.cpp
 * @author Tom Molesworth <tom@audioboundary.com>
 * @date 19/07/15 18:26:47
 *
 * 
 * $Id$
 */
#include "catch.hpp"
#include <string>
#include <queue>
#include <iostream>

class stream {
public:
	/** Additional data on the internal buffer */
	void data(const std::string &s) { data_ += s; }
	const std::string &data() const { return data_; }

	/** Sends current buffer */
	void emit() {
		// std::cout << "Emit with [" << data_ << "]\n";
		sink_(data_);
		data_ = "";
	}
	void emitter(std::function<void(const std::string &)> f) { sink_ = f; }

	virtual void incoming(const std::string &) = 0;

private:
	std::string data_;
	std::function<void(const std::string &)> sink_;
};

class char_stream : public stream {
};

class space_stream : public stream {
public:
	virtual void incoming(const std::string &s) override {
		size_t lp = 0;
		while(true) {
			size_t np = s.find(" ", lp);
			if(np == std::string::npos) {
				data(s);
				return;
			} else {
				data(s.substr(lp, np));
				emit();
			}
			lp = np;
		}
	}
};

/**
 * A delimited stream will emit every time we hit the given delimiter.
 *
 * Options:
 *
 * * Max length - if we don't see the delimiter within N chars, we fail
 * * Count - complete after N delimited chunks
 *
 */
class delim_stream : public stream {
public:
	delim_stream(const std::string &d):delim_(d),partial_("") { }

	virtual void incoming(const std::string &is) override {
		/* If we have a trailing partial delimiter, we need to check for
		 * completion on the first part of the new incoming data.
		 * This means we have:
		 * * completed data
		 * * partial incoming (up to delim_ length)
		 * * new data
		 * and our completed  data usually lags the actual data by the length
		 * of the delimiter.
		 */
		std::string s { partial_ + is };
		size_t lp = 0;
		// std::cout << "Incoming for [" << is << "], string so far [" << s << "]\n";
		while(true) {
			std::cout << "Check for [" << delim_ << "] in [" << s.substr(lp) << "]\n";
			size_t np = s.find(delim_, lp);
			if(np == std::string::npos) {
				/* No more delimiters in the string: split remaining string so we keep delimiter-1 chars for partial,
				 * the rest can go straight into data
				 */
				auto remainder = s.size() - lp;
				auto partial_len = delim_.size() - 1;
				std::cout << "Not found, stash remaining " << remainder << " chars with " << partial_len << " for partial (from " << lp << " with s.size = " << s.size() << ")\n";
				if(partial_len >= remainder) {
					partial_ = s.substr(lp);
				} else {
					data(
						s.substr(lp, remainder - partial_len)
					);
					// std::cout << "Data is now [" << data() << "]\n";
					partial_ = s.substr(lp + remainder - partial_len);
					// std::cout << "Partial is now [" << partial_ << "]\n";
				}
				return;
			} else {
				std::cout << "Add " << lp << ", " << np << "\n";
				data(s.substr(lp, np - lp));
				// std::cout << "Emit\n";
				emit();
				np += delim_.size();
			}
			lp = np;
		}
	}

private:
	const std::string delim_;
	std::string partial_;
};

SCENARIO("space-delimited streams") {
	delim_stream s(" ");
	std::queue<std::string> items;
	REQUIRE_NOTHROW(s.emitter([&items](const std::string &s) {
		// std::cout << "Had item [" << s << "]\n";
		items.push(s);
	}));
	GIVEN("an empty stream") {
		CHECK(items.empty());
		WHEN("we add a non-delimiter character") {
			REQUIRE_NOTHROW(s.incoming("a"));
			THEN("no items are emitted yet") {
				CHECK(items.empty());
			}
		}
		WHEN("we add two non-delimiter characters") {
			REQUIRE_NOTHROW(s.incoming("a"));
			REQUIRE_NOTHROW(s.incoming("b"));
			THEN("no items are emitted yet") {
				CHECK(items.empty());
			}
		}
		WHEN("we chars followed by delimiter") {
			REQUIRE_NOTHROW(s.incoming("a"));
			REQUIRE_NOTHROW(s.incoming("b"));
			REQUIRE_NOTHROW(s.incoming(" "));
			THEN("items are in the queue") {
				REQUIRE(!items.empty());
				AND_THEN("first item is correct") {
					auto v = items.front();
					items.pop();
					CHECK(v == "ab");
					AND_THEN("no more items") {
						CHECK(items.empty());
					}
				}
			}
		}
	}
}

SCENARIO("two-char-delimited streams") {
	delim_stream s("::");
	std::queue<std::string> items;
	REQUIRE_NOTHROW(s.emitter([&items](const std::string &s) {
		// std::cout << "Had item [" << s << "]\n";
		items.push(s);
	}));
	GIVEN("an empty stream") {
		CHECK(items.empty());
		WHEN("we add a non-delimiter character") {
			REQUIRE_NOTHROW(s.incoming("a"));
			THEN("no items are emitted yet") {
				CHECK(items.empty());
			}
		}
		WHEN("we add two non-delimiter characters") {
			REQUIRE_NOTHROW(s.incoming("a"));
			REQUIRE_NOTHROW(s.incoming("b"));
			THEN("no items are emitted yet") {
				CHECK(items.empty());
			}
		}
		WHEN("we add chars followed by half the delimiter") {
			REQUIRE_NOTHROW(s.incoming("a"));
			REQUIRE_NOTHROW(s.incoming("b"));
			REQUIRE_NOTHROW(s.incoming(":"));
			THEN("no items are emitted yet") {
				CHECK(items.empty());
			}
		}
		WHEN("we add chars followed by the delimiter") {
			REQUIRE_NOTHROW(s.incoming("a"));
			REQUIRE_NOTHROW(s.incoming("b"));
			REQUIRE_NOTHROW(s.incoming(":"));
			REQUIRE_NOTHROW(s.incoming(":"));
			THEN("items are in the queue") {
				REQUIRE(!items.empty());
				AND_THEN("first item is correct") {
					auto v = items.front();
					items.pop();
					CHECK(v == "ab");
					AND_THEN("no more items") {
						CHECK(items.empty());
					}
				}
			}
		}
		WHEN("we add chars followed by the full delimiter") {
			REQUIRE_NOTHROW(s.incoming("a"));
			REQUIRE_NOTHROW(s.incoming("b"));
			REQUIRE_NOTHROW(s.incoming("::"));
			THEN("items are in the queue") {
				REQUIRE(!items.empty());
				AND_THEN("first item is correct") {
					auto v = items.front();
					items.pop();
					CHECK(v == "ab");
					AND_THEN("no more items") {
						CHECK(items.empty());
					}
				}
			}
		}
		WHEN("we add two delimited chunks") {
			REQUIRE_NOTHROW(s.incoming("a:"));
			REQUIRE_NOTHROW(s.incoming(":bc::"));
			THEN("items are in the queue") {
				REQUIRE(!items.empty());
				REQUIRE(items.size() == 2);
				AND_THEN("items are correct") {
					CHECK(items.front() == "a");
					items.pop();
					CHECK(items.front() == "bc");
					items.pop();
					AND_THEN("no more items") {
						CHECK(items.empty());
					}
				}
			}
		}
	}
}
