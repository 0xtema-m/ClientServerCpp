#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include <string>
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

// Metric types
enum EMetricType {
    DOT,
    SPEED,
};

std::string ToString(EMetricType type) {
    switch (type) {
        case DOT: return "DOT";
        case SPEED: return "SPEED";
        default: return "UNKNOWN";
    }
}

struct MetricValue {
    double value;
    int64_t timestamp;
};

struct MetricIdentifiers {
    std::string project_id;
    std::vector<std::string> tags;
    EMetricType metric_type;
};

class MetricsEmulator {
public:
    MetricsEmulator(const std::string& host, unsigned short port, const std::string& project_id, const std::vector<std::string>& tags) 
        : host_(host), port_(port), project_id_(project_id), tags_(tags), ioc_(), 
          running_(false), generator_(std::random_device{}()), 
          distribution_(0.0, 100.0) {
    }

    ~MetricsEmulator() {
        stop();
    }

    bool registerProject() {
        try {
            boost::json::object json_body;
            json_body["project_id"] = project_id_;
            
            auto response = sendRequest("/register", http::verb::post, boost::json::serialize(json_body));
            
            if (response.result() != http::status::ok) {
                std::cerr << "Failed to register project: " << response.body() << std::endl;
                return false;
            }
            
            std::cout << "Project registered successfully: " << project_id_ << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error registering project: " << e.what() << std::endl;
            return false;
        }
    }

    void start() {
        if (running_) {
            return;
        }
        
        running_ = true;
        
        // Register project first
        if (!registerProject()) {
            running_ = false;
            return;
        }
        
        emulatorThread_ = std::thread([this]() {
            while (running_) {
                try {
                    sendMetrics();
                    
                    // Sleep for 15 seconds
                    for (int i = 0; i < 15 && running_; ++i) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error in emulator thread: " << e.what() << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }
        });
    }

    void stop() {
        running_ = false;
        if (emulatorThread_.joinable()) {
            emulatorThread_.join();
        }
    }

private:
    void sendMetrics() {
        int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Generate random metrics
        boost::json::object json_body;
        boost::json::array metrics_array;
        
        boost::json::object metric_json;
        metric_json["project_id"] = project_id_;
        
        // Randomly choose metric type
        EMetricType metric_type = (std::rand() % 2 == 0) ? DOT : SPEED;
        metric_json["metric_type"] = ToString(metric_type);
        
        // Generate random tags
        boost::json::array tags_array;
        for (const auto& tag : tags_) {
            tags_array.emplace_back(tag);
        }
        metric_json["tags"] = tags_array;
        
        boost::json::array values_array;
        boost::json::object value_json;
        value_json["value"] = distribution_(generator_);
        value_json["timestamp"] = now;
        values_array.emplace_back(std::move(value_json));
        metric_json["values"] = values_array;
        
        metrics_array.emplace_back(std::move(metric_json));
        
        json_body["metrics"] = metrics_array;
        
        try {
            auto response = sendRequest("/post", http::verb::post, boost::json::serialize(json_body));
            
            if (response.result() != http::status::ok) {
                std::cerr << "Failed to post metrics: " << response.body() << std::endl;
            } else {
                std::cout << "Posted 1 metric successfully at " 
                          << std::chrono::system_clock::now() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error posting metrics: " << e.what() << std::endl;
        }
    }

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
    std::string project_id_;
    std::vector<std::string> tags_;
    net::io_context ioc_;
    bool running_;
    std::thread emulatorThread_;
    std::mt19937 generator_;
    std::uniform_real_distribution<double> distribution_;
};

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    unsigned short port = 8080;
    std::string project_id = "emulator_project";
    std::vector<std::string> tags = {};
    
    // Allow overriding defaults from command line
    if (argc > 1) host = argv[1];
    if (argc > 2) port = static_cast<unsigned short>(std::stoi(argv[2]));
    if (argc > 3) project_id = argv[3];
    if (argc > 4) {
        std::string tags_str = argv[4];
        std::stringstream ss(tags_str);
        std::string tag;
        while (std::getline(ss, tag, ',')) {
            tags.push_back(tag);
        }
    }
    
    std::cout << "Starting metrics emulator for project: " << project_id << std::endl;
    std::cout << "Connecting to server at " << host << ":" << port << std::endl;
    
    MetricsEmulator emulator(host, port, project_id, tags);
    
    try {
        emulator.start();
        
        std::cout << "Emulator running. Press Enter to stop..." << std::endl;
        std::cin.get();
        
        emulator.stop();
        std::cout << "Emulator stopped." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
