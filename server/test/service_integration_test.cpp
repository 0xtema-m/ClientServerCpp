#include <gtest/gtest.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <future>
#include <string>
#include <memory>
#include <lib/service/service.h>
#include <lib/server/server.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class HttpClient {
public:
    HttpClient(const std::string& host, unsigned short port) 
        : host_(host), port_(port), ioc_() {}

    std::future<void> RegisterProject(const std::string& project_id) {
        return std::async(std::launch::async, [this, project_id]() {
            boost::json::object json_body;
            json_body["project_id"] = project_id;
            
            auto response = sendRequest("/register", http::verb::post, boost::json::serialize(json_body));
            
            if (response.result() != http::status::ok) {
                throw std::runtime_error("Failed to register project: " + response.body());
            }
        });
    }

    std::future<void> DoPost(const PostRequest& request) {
        return std::async(std::launch::async, [this, request]() {
            boost::json::object json_body;
            boost::json::array metrics_array;

            for (const auto& metric : request.metrics) {
                boost::json::object metric_json;
                metric_json["project_id"] = metric.identifiers.project_id;
                metric_json["metric_type"] = ToString(metric.identifiers.metric_type);
                
                boost::json::array tags_array;
                for (const auto& tag : metric.identifiers.tags) {
                    tags_array.emplace_back(tag);
                }
                metric_json["tags"] = tags_array;
                
                boost::json::array values_array;
                for (const auto& value : metric.values) {
                    boost::json::object value_json;
                    value_json["value"] = value.value;
                    value_json["timestamp"] = value.timestamp;
                    values_array.emplace_back(std::move(value_json));
                }
                metric_json["values"] = values_array;
                
                metrics_array.emplace_back(std::move(metric_json));
            }
            
            json_body["metrics"] = metrics_array;
            
            auto response = sendRequest("/post", http::verb::post, boost::json::serialize(json_body));
            
            if (response.result() != http::status::ok) {
                throw std::runtime_error("Failed to post metrics: " + response.body());
            }
        });
    }

    std::future<std::optional<GetResponse>> DoGet(const GetRequest& request) {
        return std::async(std::launch::async, [this, request]() -> std::optional<GetResponse> {
            boost::json::object json_body;
            json_body["project_id"] = request.identifiers.project_id;
            json_body["metric_type"] = ToString(request.identifiers.metric_type);
            
            boost::json::array tags_array;
            for (const auto& tag : request.identifiers.tags) {
                tags_array.emplace_back(tag);
            }
            json_body["tags"] = tags_array;
            
            json_body["interval_seconds"] = request.interval_seconds;
            
            auto response = sendRequest("/get", http::verb::get, boost::json::serialize(json_body));
            
            if (response.result() == http::status::not_found) {
                return std::nullopt;
            }
            
            if (response.result() != http::status::ok) {
                throw std::runtime_error("Failed to get metrics: " + response.body());
            }
            
            auto json = boost::json::parse(response.body());
            GetResponse result;
            
            if (json.as_object().contains("metrics") && json.at("metrics").is_array()) {
                for (const auto& value : json.at("metrics").as_array()) {
                    if (value.is_object() && value.as_object().contains("value") && value.as_object().contains("timestamp")) {
                        MetricValue metric_value;
                        metric_value.value = value.at("value").as_double();
                        metric_value.timestamp = value.at("timestamp").as_int64();
                        result.values.push_back(metric_value);
                    }
                }
            }
            
            return result;
        });
    }

private:
    http::response<http::string_body> sendRequest(
        const std::string& target,
        http::verb method,
        const std::string& body
    ) {
        tcp::resolver resolver(ioc_);
        beast::tcp_stream stream(ioc_);
        
        auto const results = resolver.resolve(host_, std::to_string(port_));
        stream.connect(results);
        
        http::request<http::string_body> req{method, target, 11};
        req.set(http::field::host, host_);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.set(http::field::content_type, "application/json");
        req.body() = body;
        req.prepare_payload();
        
        http::write(stream, req);
        
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        
        if (ec && ec != beast::errc::not_connected) {
            throw beast::system_error{ec};
        }
        
        return res;
    }

    std::string host_;
    unsigned short port_;
    net::io_context ioc_;
};

// Test fixture for setting up and tearing down PostgreSQL in a Docker container
class DockerPostgresFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(std::system("podman run --name tsdb-test -e POSTGRES_PASSWORD=yourpassword -e POSTGRES_DB=tsdb -d -p 5432:5432 timescale/timescaledb-ha:pg17"), 0);
        std::this_thread::sleep_for(std::chrono::seconds(5));
        pqxx::connection connection("host=localhost user=postgres password=yourpassword dbname=tsdb port=5432");
        pqxx::work tx(connection);
        tx.exec("CREATE EXTENSION IF NOT EXISTS timescaledb;");
        tx.commit();
        
        // Start the HTTP server in a separate thread
        serverThread_ = std::thread([this]() {
            net::thread_pool pool(1);
            
            listener_ = std::make_shared<HttpListener>(
                ioc_for_server_,
                tcp::endpoint{net::ip::make_address("127.0.0.1"), 8080},
                std::ref(pool));
                
            listener_->run();

            ioc_for_server_.run();
            pool.join();
        });
        
        // Give the server time to start
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Initialize HTTP client
        client_ = std::make_unique<HttpClient>("127.0.0.1", 8080);
    }

    void TearDown() override {
        listener_->stop();
        ioc_for_server_.stop();
        ioc_.stop();
        if (serverThread_.joinable()) {
            serverThread_.join();
        }
        
        std::system("podman stop tsdb-test");
        std::system("podman rm tsdb-test");
    }
    
    net::io_context ioc_;
    net::io_context ioc_for_server_;
    std::thread serverThread_;
    std::unique_ptr<HttpClient> client_;
    std::shared_ptr<HttpListener> listener_;
};

TEST_F(DockerPostgresFixture, PostAndGetMetrics) {
    MetricIdentifiers ids;
    ids.project_id = "test_project";
    ids.tags = {"tag1", "tag2"};
    ids.metric_type = EMetricType::DOT;  // Set a default metric type

    client_->RegisterProject(ids.project_id).get();
    
    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() / 15000 * 15000;

    std::vector<MetricValue> values;
    values.push_back({10.5, now - 30000});
    values.push_back({20.5, now - 15000});

    std::vector<std::future<void>> futures;
    for (const auto& value: values) {
        PostRequest postRequest;
        postRequest.metrics.push_back({ids, {value}});
        futures.push_back(client_->DoPost(postRequest));
    }

    values.clear();
    values.push_back({10.0, now});
    values.push_back({30.5, now + 5000});
    values.push_back({10.5, now + 10000});
    PostRequest postRequest;
    postRequest.metrics.push_back({ids, values});
    futures.push_back(client_->DoPost(postRequest));

    for (auto& future: futures) {
        future.get();
    }

    GetRequest getRequest;
    getRequest.identifiers = ids;
    getRequest.interval_seconds = 60;

    auto response = client_->DoGet(getRequest).get();

    ASSERT_TRUE(response.has_value());
    ASSERT_EQ(response->values.size(), 3);
    
    EXPECT_NEAR(response->values[0].value, 10.5, 0.001);
    EXPECT_NEAR(response->values[1].value, 20.5, 0.001);
    EXPECT_NEAR(response->values[2].value, 51.0, 0.001);
} 