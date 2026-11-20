
#include "HttpParser.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t MAX_HEADER_SIZE = 16384;
constexpr std::size_t MAX_BODY_SIZE = 1024 * 1024;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t");

    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t");

    return value.substr(first, last - first + 1);
}

std::string lower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))
        );
    }

    return value;
}

bool containsToken(const std::string& value,
                   const std::string& token) {
    std::size_t start = 0;

    while (start <= value.size()) {
        std::size_t end = value.find(',', start);

        std::string part = trim(
            value.substr(start, end == std::string::npos
                                    ? std::string::npos
                                    : end - start)
        );

        if (lower(part) == token) {
            return true;
        }

        if (end == std::string::npos) {
            break;
        }

        start = end + 1;
    }

    return false;
}

bool validMethod(const std::string& method) {
    if (method.empty()) {
        return false;
    }

    for (unsigned char c : method) {
        if (!std::isalnum(c) && c != '-') {
            return false;
        }
    }

    return true;
}

bool validTarget(const std::string& target) {
    if (target.empty()) {
        return false;
    }

    for (unsigned char c : target) {
        if (c <= 32 || c == 127) {
            return false;
        }
    }

    return true;
}

} 

ParseResult HttpParser::parse(const std::string& data) {
    ParseResult result{
        ParseStatus::incomplete,
        {},
        0
    };

    const std::size_t headerEnd = data.find("\r\n\r\n");

    if (headerEnd == std::string::npos) {
        if (data.size() > MAX_HEADER_SIZE) {
            result.status = ParseStatus::bad_request;
        }

        return result;
    }

    if (headerEnd + 4 > MAX_HEADER_SIZE) {
        result.status = ParseStatus::bad_request;
        return result;
    }

    const std::size_t lineEnd = data.find("\r\n");

    if (lineEnd == std::string::npos || lineEnd > headerEnd) {
        result.status = ParseStatus::bad_request;
        return result;
    }

    const std::string requestLine = data.substr(0, lineEnd);

    const std::size_t firstSpace = requestLine.find(' ');
    const std::size_t secondSpace =
        firstSpace == std::string::npos
            ? std::string::npos
            : requestLine.find(' ', firstSpace + 1);

    if (firstSpace == std::string::npos ||
        secondSpace == std::string::npos ||
        requestLine.find(' ', secondSpace + 1) != std::string::npos) {
        result.status = ParseStatus::bad_request;
        return result;
    }

    HttpRequest request;

    request.method = requestLine.substr(0, firstSpace);

    request.path = requestLine.substr(
        firstSpace + 1,
        secondSpace - firstSpace - 1
    );

    request.version = requestLine.substr(secondSpace + 1);

    if (!validMethod(request.method) ||
        !validTarget(request.path) ||
        (request.version != "HTTP/1.1" &&
         request.version != "HTTP/1.0")) {
        result.status = ParseStatus::bad_request;
        return result;
    }

    request.keepAlive = request.version == "HTTP/1.1";

    std::size_t pos = lineEnd + 2;

    while (pos < headerEnd) {
        const std::size_t next = data.find("\r\n", pos);

        if (next == std::string::npos || next > headerEnd) {
            result.status = ParseStatus::bad_request;
            return result;
        }

        const std::string line = data.substr(pos, next - pos);
        const std::size_t colon = line.find(':');

        if (colon == std::string::npos || colon == 0) {
            result.status = ParseStatus::bad_request;
            return result;
        }

        const std::string name = lower(line.substr(0, colon));

        for (unsigned char c : name) {
            if (!std::isalnum(c) && c != '-') {
                result.status = ParseStatus::bad_request;
                return result;
            }
        }

        const std::string value = trim(line.substr(colon + 1));

        if (request.headers.contains(name)) {
            result.status = ParseStatus::bad_request;
            return result;
        }

        request.headers.emplace(name, value);

        pos = next + 2;
    }

    std::size_t bodySize = 0;

    auto contentLength = request.headers.find("content-length");

    if (contentLength != request.headers.end()) {
        const std::string& value = contentLength->second;

        if (value.empty() ||
            !std::all_of(value.begin(), value.end(),
                         [](unsigned char c) {
                             return std::isdigit(c);
                         })) {
            result.status = ParseStatus::bad_request;
            return result;
        }

        try {
            const unsigned long long parsed =
                std::stoull(value);

            if (parsed > MAX_BODY_SIZE) {
                result.status = ParseStatus::bad_request;
                return result;
            }

            bodySize = static_cast<std::size_t>(parsed);
        } catch (const std::exception&) {
            result.status = ParseStatus::bad_request;
            return result;
        }
    }

    if (request.headers.contains("transfer-encoding")) {
        result.status = ParseStatus::bad_request;
        return result;
    }

    const std::size_t requestSize = headerEnd + 4 + bodySize;

    if (data.size() < requestSize) {
        return result;
    }

    request.body = data.substr(headerEnd + 4, bodySize);

    auto connection = request.headers.find("connection");

    if (connection != request.headers.end()) {
        const std::string value = lower(connection->second);

        if (containsToken(value, "close")) {
            request.keepAlive = false;
        } else if (containsToken(value, "keep-alive")) {
            request.keepAlive = true;
        }
    }

    result.status = ParseStatus::complete;
    result.request = std::move(request);
    result.consumed = requestSize;

    return result;
}
