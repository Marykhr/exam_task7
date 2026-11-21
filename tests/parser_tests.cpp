
#include "http/HttpParser.hpp"

#include <gtest/gtest.h>

TEST(HttpParserTests, ParseGetRequest) {
    const std::string data =
        "GET /health HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    auto result = HttpParser::parse(data);

    ASSERT_EQ(result.status, ParseStatus::complete);

    EXPECT_EQ(result.request.method, "GET");
    EXPECT_EQ(result.request.path, "/health");
    EXPECT_EQ(result.request.version, "HTTP/1.1");
    EXPECT_TRUE(result.request.keepAlive);
}

TEST(HttpParserTests, ParsePostWithJsonBody) {
    const std::string body = R"({"name":"Apple","value":10})";

    const std::string data =
        "POST /items HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) +
        "\r\n\r\n" + body;

    auto result = HttpParser::parse(data);

    ASSERT_EQ(result.status, ParseStatus::complete);

    EXPECT_EQ(result.request.method, "POST");
    EXPECT_EQ(result.request.path, "/items");
    EXPECT_EQ(result.request.body, body);
}

TEST(HttpParserTests, IncompleteHeaders) {
    const std::string data =
        "GET /health HTTP/1.1\r\n"
        "Host: localhost\r\n";

    auto result = HttpParser::parse(data);

    EXPECT_EQ(result.status, ParseStatus::incomplete);
}

TEST(HttpParserTests, IncompleteBody) {
    const std::string data =
        "POST /items HTTP/1.1\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "hello";

    auto result = HttpParser::parse(data);

    EXPECT_EQ(result.status, ParseStatus::incomplete);
}

TEST(HttpParserTests, BadRequestLine) {
    const std::string data =
        "INVALID REQUEST\r\n\r\n";

    auto result = HttpParser::parse(data);

    EXPECT_EQ(result.status, ParseStatus::bad_request);
}

TEST(HttpParserTests, KeepAliveDisabled) {
    const std::string data =
        "GET /health HTTP/1.1\r\n"
        "Connection: close\r\n"
        "\r\n";

    auto result = HttpParser::parse(data);

    ASSERT_EQ(result.status, ParseStatus::complete);
    EXPECT_FALSE(result.request.keepAlive);
}

TEST(HttpParserTests, ParseHttp10) {
    const std::string data =
        "GET /health HTTP/1.0\r\n"
        "\r\n";

    auto result = HttpParser::parse(data);

    ASSERT_EQ(result.status, ParseStatus::complete);
    EXPECT_FALSE(result.request.keepAlive);
}

TEST(HttpParserTests, ContentLengthInvalid) {
    const std::string data =
        "POST /items HTTP/1.1\r\n"
        "Content-Length: abc\r\n"
        "\r\n";

    auto result = HttpParser::parse(data);

    EXPECT_EQ(result.status, ParseStatus::bad_request);
}

TEST(HttpParserTests, MultipleRequestsInBuffer) {
    const std::string first =
        "GET /health HTTP/1.1\r\n\r\n";

    const std::string second =
        "GET /items HTTP/1.1\r\n\r\n";

    const std::string data = first + second;

    auto result = HttpParser::parse(data);

    ASSERT_EQ(result.status, ParseStatus::complete);
    EXPECT_EQ(result.request.path, "/health");
    EXPECT_EQ(result.consumed, first.size());

    auto next = HttpParser::parse(data.substr(result.consumed));

    ASSERT_EQ(next.status, ParseStatus::complete);
    EXPECT_EQ(next.request.path, "/items");
}
