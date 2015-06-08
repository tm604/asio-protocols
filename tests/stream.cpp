#define BOOST_TEST_MAIN
#if !defined( WIN32 )
    #define BOOST_TEST_DYN_LINK
#endif
#define BOOST_TEST_MODULE StreamTests

#include <boost/test/unit_test.hpp>

#include "net/asio/stream.h"
#include "Log.h"

using namespace std;
using namespace net;

BOOST_AUTO_TEST_CASE(streaming)
{
#if 0
	auto s = stream {};
#endif
	BOOST_CHECK(true);
}

