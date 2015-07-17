void
example()
{
	auto client = net::asio::tcp::client();
	client->connect(
		"localhost",
		80
	)->then(
		[](std::shared_ptr<stream> s) {
			s->sink([](const std::string &in) {
				std::cout << in;
			});
			return s->write(
				"GET / HTTP/1.1\x0D\x0A\x0D\x0A"
			)->then(
				[s](size_t written) {
					return s->remote_eof();
				}
			);
		}
	);
}

