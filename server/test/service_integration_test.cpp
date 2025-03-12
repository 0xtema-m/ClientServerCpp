#include <gtest/gtest.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <lib/server/server.h>

class DockerPostgresFixture : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(std::system("docker run --name tsdb-test -e POSTGRES_PASSWORD=yourpassword -e POSTGRES_DB=tsdb -d -p 5432:5432 timescale/timescaledb-ha:pg17"), 0);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    void TearDown() override {
        std::system("docker stop tsdb-test");
        std::system("docker rm tsdb-test");
    }
};

TEST_F(DockerPostgresFixture, PostAndGetMetrics) {
    Server server;
    
    PostRequest postRequest;
    
    MetricIdentifiers ids;
    ids.project_id = "test_project";
    ids.tags = {"tag1", "tag2"};
    
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() / 15000 * 15000;

    std::vector<MetricValue> values;
    values.push_back({10.5, now - 30000});
    values.push_back({20.5, now - 15000});
    values.push_back({10.0, now});
    values.push_back({30.5, now + 5000});
    values.push_back({10.5, now + 10000});

    postRequest.metrics.push_back({ids, values});

    server.DoPost(postRequest).get();
    
    GetRequest getRequest;
    getRequest.identifiers = ids;
    getRequest.interval_seconds = 60;

    auto response = server.DoGet(getRequest).get();

    ASSERT_TRUE(response.has_value());
    ASSERT_EQ(response->values.size(), 3);
    
    EXPECT_NEAR(response->values[0].value, 10.5, 0.001);
    EXPECT_NEAR(response->values[1].value, 20.5, 0.001);
    EXPECT_NEAR(response->values[2].value, 51.0, 0.001);
} 