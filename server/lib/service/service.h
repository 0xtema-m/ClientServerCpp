#pragma once

#include <pqxx/pqxx>
#include <boost/functional/hash.hpp>
#include <iostream>
#include <numeric>
#include <utility>
#include <format>
#include <optional>
#include <vector>
#include <string>
#include <map>

using Tags = std::vector<std::string>;

enum EMetricType {
    DOT,
    SPEED,
};

std::string ToString(EMetricType type);
EMetricType FromString(const std::string& str);

struct MetricIdentifiers {
    std::string project_id;
    Tags tags;
    EMetricType metric_type;

    bool operator==(const MetricIdentifiers &other) const = default;
};

struct MetricIdentifiersHasher {
    std::size_t operator()(const MetricIdentifiers& ids) const {
        std::size_t seed = 0;
        boost::hash_combine(seed, boost::hash_value(ids.project_id));
        boost::hash_combine(seed, boost::hash_value(ids.tags));
        boost::hash_combine(seed, boost::hash_value(ids.metric_type));
        return seed;
    }
};

struct MetricValue {
    double value;
    uint64_t timestamp;
};

struct Metric {
    MetricIdentifiers identifiers;
    std::vector<MetricValue> values;
};

struct PostRequest {
    std::vector<Metric> metrics;
};

struct GetRequest {
    MetricIdentifiers identifiers;
    uint64_t interval_seconds;
};

struct GetResponse {
    std::vector<MetricValue> values;
};

class MonitoringService {
public:
    MonitoringService();

    void DoPost(const PostRequest& request);
    std::optional<GetResponse> DoGet(const GetRequest& request);

private:
    pqxx::connection m_connection;
};
