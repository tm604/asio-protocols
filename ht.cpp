#include <net/asio/http.h>

int
main(int argc, const char *argv[])
{
	boost::asio::io_service service { };
	auto client_ = std::make_shared<net::http::client>(service);
	bool show_request = false;
	bool show_headers = false;
	for(int i = 1; i < argc; ++i) {
		auto v = std::string { argv[i] };
		if(v == "--headers") {
			show_headers = true;
		} else if(v == "--request") {
			show_request = true;
		} else {
			auto req = net::http::request {
				net::http::uri {
					argv[i]
				}
			};
			req << net::http::header("User-agent", "some-user-agent");
			auto res = client_->GET(
				std::move(req)
			);
			res->completion()->on_done([i, show_request, show_headers](const std::shared_ptr<net::http::response> &res) {
				if(show_request) {
					std::cout << res->request().bytes() << "\n";
				}
				if(show_headers) {
					res->each_header([](const net::http::header &h) {
						std::cout << h.key() << ": " << h.value() << "\n";
					});
					std::cout << "\n";
				}
				std::cout << res->body();
			});
		}
	}

	// std::cout << "run\n";
	service.run();
	// std::cout << "done\n";
}

