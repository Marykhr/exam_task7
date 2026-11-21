
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;

    std::unordered_map<std::string, std::string> headers;

    std::string body;
    bool keepAlive = false;
};

enum class ParseStatus {
    incomplete,
    complete,
    bad_request
};

struct ParseResult {
    ParseStatus status;
    HttpRequest request;
    std::size_t consumed = 0;
};

class HttpParser {
public:
    static ParseResult parse(const std::string& data);
};
