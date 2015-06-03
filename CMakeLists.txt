cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

PROJECT(asio-protocols)

enable_testing()
set (VERSION_MAJOR 1)
set (VERSION_MINOR 0)
set(Protocols_LIBRARY asio-protocols)

set(Boost_USE_MULTITHREADED ON)
add_definitions(-DBOOST_LOG_DYN_LINK)

include_directories(include/ src/)

find_package(Boost COMPONENTS chrono system regex thread log unit_test_framework)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(Future REQUIRED)

find_package(AMQP REQUIRED)
include_directories(${AMQP_INCLUDE_DIR})

add_library(
	libasio-protocols.a
	src/amqp.cpp
)

add_executable(
	amqp_tests
	src/amqp_tests.cpp
)

target_link_libraries(
	amqp_tests
	${AMQP_LIBRARY}
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
	${OPENSSL_CRYPTO_LIBRARY}
	${OPENSSL_SSL_LIBRARY}
	z
)
if(WIN32)
	target_link_libraries(amqp_tests wsock32 ws2_32)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

install(
	DIRECTORY include/
	DESTINATION "include"
)
install(
	TARGETS ${Protocols_LIBRARY}
	ARCHIVE DESTINATION lib
)

include (CTest)

add_test (amqp_tests amqp_tests --log_level=all --log_format=XML --log_sink=amqp_tests.xml)

