
#include "Storage.hpp"

#include <algorithm>
#include <limits>
#include <utility>

Item Storage::create(const std::string& name, std::int64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);

    Item item{
        nextId_++,
        name,
        value,
        1
    };

    items_.emplace(item.id, item);

    return item;
}

std::optional<Item> Storage::get(std::uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(id);

    if (it == items_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<Item> Storage::getAll(
    std::size_t offset,
    std::size_t limit
) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Item> result;
    result.reserve(items_.size());

    for (const auto& [id, item] : items_) {
        result.push_back(item);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const Item& a, const Item& b) {
            return a.id < b.id;
        }
    );

    if (offset >= result.size() || limit == 0) {
        return {};
    }

    auto first = result.begin() + offset;

    auto last = result.begin() +
        offset + std::min(limit, result.size() - offset);

    return std::vector<Item>(first, last);
}

StorageResult Storage::replace(
    std::uint64_t id,
    const std::string& name,
    std::int64_t value,
    std::optional<std::uint64_t> expectedVersion
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(id);

    if (it == items_.end()) {
        return {StorageStatus::not_found, {}};
    }

    Item& item = it->second;

    if (expectedVersion && *expectedVersion != item.version) {
        return {StorageStatus::version_conflict, item};
    }

    item.name = name;
    item.value = value;
    ++item.version;

    return {StorageStatus::ok, item};
}

StorageResult Storage::patch(
    std::uint64_t id,
    std::optional<std::string> name,
    std::optional<std::int64_t> value,
    std::optional<std::uint64_t> expectedVersion
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(id);

    if (it == items_.end()) {
        return {StorageStatus::not_found, {}};
    }

    Item& item = it->second;

    if (expectedVersion && *expectedVersion != item.version) {
        return {StorageStatus::version_conflict, item};
    }

    if (name) {
        item.name = *name;
    }

    if (value) {
        item.value = *value;
    }

    ++item.version;

    return {StorageStatus::ok, item};
}

bool Storage::remove(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);

    return items_.erase(id) > 0;
}

StorageResult Storage::increment(
    std::uint64_t id,
    std::int64_t delta
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = items_.find(id);

    if (it == items_.end()) {
        return {StorageStatus::not_found, {}};
    }

    Item& item = it->second;

    if ((delta > 0 &&
         item.value > std::numeric_limits<std::int64_t>::max() - delta) ||
        (delta < 0 &&
         item.value < std::numeric_limits<std::int64_t>::min() - delta)) {
        return {StorageStatus::overflow, item};
    }

    item.value += delta;
    ++item.version;

    return {StorageStatus::ok, item};
}
