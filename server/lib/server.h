#pragma once

#include <boost/asio/thread_pool.hpp>

#include <thread>
#include <variant>

enum TypeOfMetric {
    DOT = 0,
    DERIVATIVE = 1,
};

using MetricTag = std::pair<std::string, std::string>;
using MetricTags = std::vector<MetricTag>;

struct Metric {
    MetricTags metric_tags;
    TypeOfMetric type_of_metric;
    double value;
    uint64_t timestamp_ms;
};

template<typename T, typename GetMetricsRequest, typename PostMetricsRequest>
concept Connection = requires(T connection, std::variant<typename GetMetricsRequest::Response, typename GetMetricsRequest::Response> response) {
    { connection.GetRequest() } -> std::convertible_to<std::variant<GetMetricsRequest, PostMetricsRequest>>;
    { connection.SetResponse(std::move(response)) };
    { connection.Close() };
};

template<typename T, typename GetMetricsRequest, typename PostMetricsRequest, typename ConcreteConnection>
concept Acceptor = Connection<ConcreteConnection, GetMetricsRequest, PostMetricsRequest> && requires(T acceptor) {
    // Connection<GetMetricsRequest, PostMetricsRequest>
    { acceptor.Accept() } -> std::convertible_to<ConcreteConnection>;
};


// template<typename PostMetricsRequest, typename GetMetricsRequest, MonitoringService<PostMetricsRequest, GetMetricsRequest> ConcreteMonitoringService>
// class Server;

int Main(int argc, char** argv);
