
#pragma once

#include <cstdint>
#include <string>

struct Item {
    std::uint64_t id;
    std::string name;
    std::int64_t value;
    std::uint64_t version;
};
