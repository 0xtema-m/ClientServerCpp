#include <lib/server/server.h>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr <<
            "Usage: http-server-async <address> <port> <threads>\n" <<
            "Example:\n" <<
            "    http-server-async 0.0.0.0 8080 4\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    net::io_context ioc{threads};

    net::thread_pool thread_pool(threads);

    std::make_shared<HttpListener>(
        ioc,
        tcp::endpoint{address, port},
        std::ref(thread_pool))->run();

    std::vector<std::thread> v;
    std::generate_n(
        std::back_inserter(v),
        threads - 1,
        [&ioc]() {
            return std::thread([&ioc] {
                ioc.run();
            });
        });
    ioc.run();

    std::for_each(v.begin(), v.end(), [](std::thread& t) {
        t.join();
    });

    thread_pool.join();

    return EXIT_SUCCESS;
}
