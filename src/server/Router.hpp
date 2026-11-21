
#pragma once

#include "storage/Storage.hpp"
#include "http/HttpParser.hpp"

#include <string>

class Router {
public:
    explicit Router(Storage& storage);

    std::string handle(const HttpRequest& request);

private:
    Storage& storage_;
};