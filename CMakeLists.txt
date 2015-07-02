cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

PROJECT(
	asio-protocols
)

enable_testing()
set (VERSION_MAJOR 1)
set (VERSION_MINOR 0)
set (VERSION_PATCH 3)
set (PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
set (PROJECT_VERSION_REVISION 2)

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
	${Protocols_LIBRARY}
	src/buffer.cpp
	src/source.cpp
	src/sink.cpp
	src/stream.cpp
	src/amqp.cpp
	src/http.cpp
)

add_executable(
	stream_tests
	tests/stream.cpp
)

target_link_libraries(
	stream_tests
	${Protocols_LIBRARY}
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
	${OPENSSL_CRYPTO_LIBRARY}
	${OPENSSL_SSL_LIBRARY}
	z
)
if(WIN32)
	target_link_libraries(stream_tests wsock32 ws2_32)
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_executable(
	amqp_tests
	src/amqp_tests.cpp
)

target_link_libraries(
	amqp_tests
	${Protocols_LIBRARY}
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

add_executable(
	http_tests
	tests/http.cpp
)

target_link_libraries(
	http_tests
	${Protocols_LIBRARY}
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
	${OPENSSL_CRYPTO_LIBRARY}
	${OPENSSL_SSL_LIBRARY}
	z
)
if(WIN32)
	target_link_libraries(http_tests wsock32 ws2_32)
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
add_test (http_tests http_tests --log_level=all --log_format=XML --log_sink=http_tests.xml)
add_test (stream_tests stream_tests --log_level=all --log_format=XML --log_sink=stream_tests.xml)

include (InstallRequiredSystemLibraries)
set (CPACK_PACKAGE_NAME "libasio-protocols")
set (CPACK_PACKAGE_VENDOR "")
set (CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set (CPACK_PACKAGE_CONTACT "Tom Molesworth <tom@perlsite.co.uk>")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "C++ network protocols implementation using ASIO")
set (CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.debian")
set (CPACK_GENERATOR "TGZ;DEB")
string(TOLOWER "${CPACK_PACKAGE_NAME}" CPACK_PACKAGE_NAME_LOWERCASE)
find_program(DPKG_PROGRAM dpkg)
if(DPKG_PROGRAM)
	execute_process(
		COMMAND ${DPKG_PROGRAM} --print-architecture
		OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
		OUTPUT_STRIP_TRAILING_WHITESPACE
		)
	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME_LOWERCASE}_${PROJECT_VERSION}-${PROJECT_VERSION_REVISION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")   
else(DPKG_PROGRAM)
	set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME_LOWERCASE}_${PROJECT_VERSION}-${PROJECT_VERSION_REVISION}_${CMAKE_SYSTEM_NAME}")
endif(DPKG_PROGRAM)
# set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS "ON")
include (CPack)
