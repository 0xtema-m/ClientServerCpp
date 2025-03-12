#pragma once

#include <lib/service/service.h>

#include <boost/asio.hpp>

#include <thread>

// no networking, just thread pool
class Server {
public:
    Server()
        : m_thread_pool(std::thread::hardware_concurrency())
    {
    }

    std::future<void> RegisterProject(const RegisterProjectRequest& request) {
        std::promise<void> promise;
        auto future = promise.get_future();
        boost::asio::post(m_thread_pool, [request, promise = std::move(promise)]() mutable {
            MonitoringService service;
            service.RegisterProject(request);
            promise.set_value();
        });
        return future;
    }

    std::future<void> DoPost(const PostRequest& request) {
        std::promise<void> promise;
        auto future = promise.get_future();
        boost::asio::post(m_thread_pool, [request, promise = std::move(promise)]() mutable {
            MonitoringService service;
            service.DoPost(request);
            promise.set_value();
        });
        return future;
    }

    std::future<std::optional<GetResponse>> DoGet(const GetRequest& request) {
        std::promise<std::optional<GetResponse>> promise;
        auto future = promise.get_future();
        boost::asio::post(m_thread_pool, [request, promise = std::move(promise)]() mutable {
            MonitoringService service;
            auto response = service.DoGet(request);
            promise.set_value(std::move(response));
        });
        return future;
    }

private:
    boost::asio::thread_pool m_thread_pool;
};
