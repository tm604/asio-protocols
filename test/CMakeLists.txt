include_directories(deps/)
add_executable(
    asio_protocols_unit_tests
	main.cpp
	http.cpp
	statsd.cpp
	streams.cpp
	# transport/http.cpp
)
target_link_libraries(
    asio_protocols_unit_tests
	${Protocols_LIBRARY}
	${Boost_LIBRARIES}
	${OPENSSL_LIBRARIES}
	${OPENSSL_CRYPTO_LIBRARY}
	${OPENSSL_SSL_LIBRARY}
	z
	${CPS_FUTURE_LIBRARIES}
	${ICONV_LIBRARIES}
)
# win32 needs extra things for network access
if(WIN32)
    target_link_libraries(
		asio_protocols_unit_tests
        wsock32
        ws2_32
    )
endif()

add_test (asio_protocols_unit_tests asio_protocols_unit_tests -r junit -o asio_protocols_unit_tests.xml)

