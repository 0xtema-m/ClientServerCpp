#include "server.h"

#include <iostream>

#include <boost/asio/post.hpp>

int Main(int argc, char** argv) {
    boost::asio::thread_pool tp(1);
    boost::asio::post(
        tp,
        []() {
            std::cout << "Hello world!" << std::endl;
        }
    );

    tp.join();
    return 0;
}
