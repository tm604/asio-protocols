cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

PROJECT(asio-amqp)

enable_testing()
set (VERSION_MAJOR 1)
set (VERSION_MINOR 0)

set(Boost_USE_MULTITHREADED ON)
add_definitions(-DBOOST_LOG_DYN_LINK)

include(cotire)
include_directories(include/ src/)

find_package(Boost COMPONENTS system regex thread log unit_test_framework)
find_package(Threads REQUIRED)

find_package(AMQP REQUIRED)
include_directories(${AMQP_INCLUDE_DIR})

add_executable(
	amqp_tests
	src/amqp_tests.cpp
)

target_link_libraries(
	amqp_tests
	${AMQP_LIBRARY}
	${Boost_LIBRARIES}
)
set_target_properties(amqp_tests PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/stdafx.h")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
 # target_compile_features(amqp_tests PRIVATE cxx_constexpr)
cotire(amqp_tests)

install(
	DIRECTORY include/
	DESTINATION "include"
)
include (CTest)
add_test (boost amqp_tests --log_level=all --log_format=XML --log_sink=amqp_tests.xml)

