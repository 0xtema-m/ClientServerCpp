#include "service.h"

std::string ToString(EMetricType type) {
    switch (type) {
        case EMetricType::DOT:
            return "DOT";
        case EMetricType::SPEED:
            return "SPEED";
        default:
            std::unreachable();
    }
}

EMetricType FromString(const std::string& str) {
    if (str == "DOT") {
        return EMetricType::DOT;
    }
    if (str == "SPEED") {
        return EMetricType::SPEED;
    }
    std::unreachable();
}

MonitoringService::MonitoringService()
    : m_connection(
        "host=localhost "
        "dbname=tsdb "
        "user=postgres "
        "password=yourpassword "
        "port=5432"
    )
{
    pqxx::work tx(m_connection);
    tx.exec("CREATE EXTENSION IF NOT EXISTS timescaledb;");
    tx.commit();
}

void MonitoringService::DoPost(const PostRequest& request) {
    if (!m_connection.is_open()) {
        std::cerr << "Connection is closed" << std::endl;
        return;
    }

    pqxx::work tx(m_connection);
    for (const auto& [ids, value]: request.metrics) {
        std::string table_name = tx.quote_name(ids.project_id);
        
        tx.exec(std::format(R"(
            CREATE TABLE IF NOT EXISTS {} (
                time   TIMESTAMPTZ         NOT NULL,
                tags   TEXT                NOT NULL,
                value  DOUBLE PRECISION    NULL
            );
        )", table_name));

        tx.exec(std::format(R"(
            SELECT create_hypertable('{}', 'time', if_not_exists => TRUE);
        )", table_name));

        auto tags_str = std::accumulate(
            ids.tags.begin(), ids.tags.end(), std::string(""),
            [](std::string&& accumulated, const std::string& next) {
                accumulated += '|';
                accumulated += next;
                return std::move(accumulated);
            }
        );

        std::map<uint64_t, double> aggregated_values;
        for (const auto& metric_value : value) {
            uint64_t bucket = (metric_value.timestamp / 15000) * 15000;
            aggregated_values[bucket] += metric_value.value;
        }

        for (const auto& [bucket_ts, sum_value] : aggregated_values) {
            std::string timestamp_ms = tx.quote(bucket_ts);
            std::string quoted_tags = tx.quote(tags_str);
            std::string quoted_value = tx.quote(sum_value);

            tx.exec(std::format(R"(
                INSERT INTO {} (time, tags, value)
                VALUES (
                    to_timestamp({}::bigint / 1000.0),
                    {},
                    {}
                )
            )", table_name, timestamp_ms, quoted_tags, quoted_value));
        }
    }
    tx.commit();
}

std::optional<GetResponse> MonitoringService::DoGet(const GetRequest& request) {
    if (!m_connection.is_open()) {
        std::cerr << "Connection is closed" << std::endl;
        return std::nullopt;
    }

    pqxx::work tx(m_connection);
    std::string table_name = tx.quote_name(request.identifiers.project_id);

    auto tags_str = std::accumulate(
        request.identifiers.tags.begin(), 
        request.identifiers.tags.end(), 
        std::string(""),
        [](std::string&& accumulated, const std::string& next) {
            accumulated += '|';
            accumulated += next;
            return std::move(accumulated);
        }
    );

    auto result = tx.exec(
        "SELECT "
        "    (EXTRACT(EPOCH FROM time) * 1000)::bigint as time_ms, "
        "    value "
        "FROM " + table_name + 
        " WHERE tags = " + tx.quote(tags_str) +
        " AND time > NOW() - INTERVAL '" + std::to_string(request.interval_seconds) + " seconds'" +
        " ORDER BY time ASC"
    );

    if (result.empty()) {
        return std::nullopt;
    }

    GetResponse response;
    response.values.reserve(result.size());

    for (const auto& row : result) {
        MetricValue value;
        value.timestamp = row[0].as<uint64_t>();
        value.value = row[1].as<double>();
        response.values.push_back(value);
    }

    tx.commit();
    return response;
}
