#pragma once

#include <boost/container_hash/hash.hpp>
#include <lib/service/service.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/config.hpp>
#include <boost/json.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace {

    inline RegisterProjectRequest ParseRegisterProjectRequest(const std::string& body) {
        auto json = boost::json::parse(body);
        return RegisterProjectRequest{
            .project_id = json.at("project_id").as_string().c_str()
        };
    }

    inline PostRequest ParsePostRequest(const std::string& body) {
        auto json = boost::json::parse(body);
        PostRequest request;
        for (auto& entry : json.at("metrics").as_array()) {
            auto metric_json = entry.as_object();
            request.metrics.emplace_back();
            auto& metric = request.metrics.back();
            metric.identifiers.project_id = metric_json.at("project_id").as_string().c_str();
            metric.identifiers.metric_type = FromString(metric_json.at("metric_type").as_string().c_str());
            for (auto& tag : metric_json.at("tags").as_array()) {
                metric.identifiers.tags.push_back(tag.as_string().c_str());
            }
            for (auto& value : metric_json.at("values").as_array()) {
                metric.values.push_back(MetricValue{
                    .value = value.at("value").as_double(),
                    .timestamp = value.at("timestamp").as_int64()
                });
            }
        }
        return request;
    }

    inline GetRequest ParseGetRequest(const std::string& body) {
        auto json = boost::json::parse(body);
        GetRequest request;
        request.identifiers.project_id = json.at("project_id").as_string().c_str();
        request.identifiers.metric_type = FromString(json.at("metric_type").as_string().c_str());
        for (auto& tag : json.at("tags").as_array()) {
            request.identifiers.tags.push_back(tag.as_string().c_str());
        }
        request.interval_seconds = json.at("interval_seconds").as_int64();
        return request;
    }

    inline std::string GetResponseToJson(const GetResponse& response) {
        boost::json::object json;
        json["metrics"] = boost::json::array();
        boost::json::array& metrics = json["metrics"].as_array();
        for (auto& value : response.values) {
            metrics.push_back(boost::json::object{
                {"value", value.value},
                {"timestamp", value.timestamp}
            });
        }
        return boost::json::serialize(json);
    }

} // anonymous namespace

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket&& socket, std::reference_wrapper<net::thread_pool> thread_pool)
        : stream_(std::move(socket)),
          thread_pool_(thread_pool)
    {
    }

    void start() {
        req_ = {};
        stream_.expires_after(std::chrono::seconds(30));
        http::async_read(
            stream_, buffer_, req_,
            beast::bind_front_handler(&HttpSession::on_read, shared_from_this())
        );
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        
        if (ec == http::error::end_of_stream) {
            return do_close();
        }

        if (ec) {
            std::cerr << "Error: " << ec.message() << "\n";
            return;
        }

        net::post(
            thread_pool_.get(),
            beast::bind_front_handler(&HttpSession::process_request, shared_from_this())
        );
    }

    void process_request() {
        struct Handle {
            std::string name;
            boost::beast::http::verb method;

            Handle() = default;
            Handle(const std::string& name, boost::beast::http::verb method)
                : name(name),
                  method(method)
            {
            }

            Handle(http::request<http::string_body>& req)
                : name(req.target()),
                  method(req.method())
            {
            }

            bool operator==(const Handle& other) const = default;
        };
        std::unordered_map<
            Handle,
            std::function<void(http::request<http::string_body>&, http::response<http::string_body>&)>,
            decltype(
                [](const Handle& h) {
                    auto base = boost::hash_value(h.name);
                    boost::hash_combine(base, h.method);
                    return base;
                }
            )
        > handlers = {
            {{"/register", http::verb::post}, &HttpSession::RegisterProject},
            {{"/post", http::verb::post}, &HttpSession::DoPost},
            {{"/get", http::verb::get}, &HttpSession::DoGet},
        };

        http::response<http::string_body> res;

        auto it = handlers.find(Handle(req_));
        if (it == handlers.end()) {
            res.result(http::status::not_found);
            res.set(http::field::content_type, "application/json");
            res.body() = "{\"message\": \"Not found handler\"}";
        } else {
            it->second(req_, res);
        }

        auto sp = std::make_shared<http::response<http::string_body>>(std::move(res));
        res_ = sp;

        http::async_write(
            stream_,
            *sp,
            beast::bind_front_handler(&HttpSession::on_write, shared_from_this(), sp->need_eof())
        );
    }

    void on_write(bool close, beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (ec) {
            std::cerr << "Error: " << ec.message() << "\n";
        }

        if (close) {
            return do_close();
        }

        start();
    }

    void do_close()
    {
        beast::error_code ec;
        boost::ignore_unused(stream_.socket().shutdown(tcp::socket::shutdown_send, ec));
        if(ec == beast::errc::not_connected) {
            ec = {};
        }
    }

    static void RegisterProject(http::request<http::string_body>& request, http::response<http::string_body>& response) {
        MonitoringService service;
        try {
            RegisterProjectRequest req = ParseRegisterProjectRequest(request.body());
            service.RegisterProject(req);
            response.result(http::status::ok);
            response.set(http::field::content_type, "application/json");
            response.body() = "{\"message\": \"Project registered successfully\"}";
        } catch (const std::exception& e) {
            response.result(http::status::bad_request);
            response.set(http::field::content_type, "application/json");
            response.body() = "{\"message\": \"" + std::string(e.what()) + "\"}";
        }
    }

    static void DoPost(http::request<http::string_body>& request, http::response<http::string_body>& response) {
        MonitoringService service;
        try {
            PostRequest req = ParsePostRequest(request.body());
            service.DoPost(req);
            response.result(http::status::ok);
            response.set(http::field::content_type, "application/json");
            response.body() = "{\"message\": \"Metrics posted successfully\"}";
        } catch (const std::exception& e) {
            response.result(http::status::bad_request);
            response.set(http::field::content_type, "application/json");
            response.body() = "{\"message\": \"" + std::string(e.what()) + "\"}";
        }
    }

    static void DoGet(http::request<http::string_body>& request, http::response<http::string_body>& response) {
        MonitoringService service;
        try {
            GetRequest req = ParseGetRequest(request.body());
            auto serviceResponse = service.DoGet(req);
            if (serviceResponse) {
                response.result(http::status::ok);
                response.set(http::field::content_type, "application/json");
                response.body() = GetResponseToJson(serviceResponse.value());
            } else {
                response.result(http::status::not_found);
                response.set(http::field::content_type, "application/json");
                response.body() = "{\"message\": \"Metrics not found\"}";
            }
        } catch (const std::exception& e) {
            response.result(http::status::bad_request);
            response.set(http::field::content_type, "application/json");
            response.body() = "{\"message\": \"" + std::string(e.what()) + "\"}";
        }
    }

private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    std::reference_wrapper<net::thread_pool> thread_pool_;
};

class HttpListener : public std::enable_shared_from_this<HttpListener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::reference_wrapper<net::thread_pool> thread_pool_;

public:
    HttpListener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        std::reference_wrapper<net::thread_pool> thread_pool
    )
        : ioc_(ioc)
        , acceptor_(ioc)
        , thread_pool_(thread_pool)
    {
        beast::error_code ec;

        boost::ignore_unused(acceptor_.open(endpoint.protocol(), ec));
        if(ec) {
            std::cerr << "open: " << ec.message() << "\n";
            return;
        }

        boost::ignore_unused(acceptor_.set_option(net::socket_base::reuse_address(true), ec));
        if(ec) {
            std::cerr << "set_option: " << ec.message() << "\n";
            return;
        }

        boost::ignore_unused(acceptor_.bind(endpoint, ec));
        if(ec) {
            std::cerr << "bind: " << ec.message() << "\n";
            return;
        }

        boost::ignore_unused(acceptor_.listen(net::socket_base::max_listen_connections, ec));
        if(ec) {
            std::cerr << "listen: " << ec.message() << "\n";
            return;
        }
    }

    void run()
    {
        if(!acceptor_.is_open())
            return;
        do_accept();
    }

    void stop() {
        beast::error_code ec;
        boost::ignore_unused(acceptor_.close(ec));
        if(ec) {
            std::cerr << "Error closing acceptor: " << ec.message() << "\n";
        }
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &HttpListener::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        if(ec) {
            std::cerr << "accept: " << ec.message() << "\n";
        } else {
            std::make_shared<HttpSession>(
                std::move(socket),
                thread_pool_)->start();
        }

        do_accept();
    }
};
