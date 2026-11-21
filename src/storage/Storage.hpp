
#pragma once

#include "Item.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

enum class StorageStatus {
    ok,
    not_found,
    version_conflict,
    overflow
};

struct StorageResult {
    StorageStatus status;
    Item item{};
};

class Storage {
public:
    Item create(const std::string& name, std::int64_t value);

    std::optional<Item> get(std::uint64_t id) const;

    std::vector<Item> getAll(
        std::size_t offset,
        std::size_t limit
    ) const;

    StorageResult replace(
        std::uint64_t id,
        const std::string& name,
        std::int64_t value,
        std::optional<std::uint64_t> expectedVersion
    );

    StorageResult patch(
        std::uint64_t id,
        std::optional<std::string> name,
        std::optional<std::int64_t> value,
        std::optional<std::uint64_t> expectedVersion
    );

    bool remove(std::uint64_t id);

    StorageResult increment(
        std::uint64_t id,
        std::int64_t delta
    );

private:
    mutable std::mutex mutex_;

    std::unordered_map<std::uint64_t, Item> items_;

    std::uint64_t nextId_ = 1;
};
