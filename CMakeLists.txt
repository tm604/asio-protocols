cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

PROJECT(asio-protocols)
set (VERSION_MAJOR 1)
set (VERSION_MINOR 0)
set (VERSION_PATCH 5)
set (PROJECT_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
set (PROJECT_VERSION_REVISION 1)

option(USE_CLANG "build application with clang" OFF)
if(USE_CLANG)
	set(CMAKE_CXX_COMPILER "/usr/bin/clang++-3.6")
endif()
include(set_cxx_norm.cmake)
set_cxx_norm(${CXX_NORM_CXX11})
enable_testing()

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

add_subdirectory(test)

add_library(
	${Protocols_LIBRARY}
	src/buffer.cpp
	src/source.cpp
	src/sink.cpp
	src/stream.cpp
	src/amqp.cpp
	#	src/http.cpp
)

install(
	DIRECTORY include/
	DESTINATION "include"
)
install(
	TARGETS ${Protocols_LIBRARY}
	ARCHIVE DESTINATION lib
)

include (CTest)

include (InstallRequiredSystemLibraries)
set (CPACK_PACKAGE_NAME "libasio-protocols")
set (CPACK_PACKAGE_VENDOR "")
set (CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
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
