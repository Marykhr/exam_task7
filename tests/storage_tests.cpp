
#include "storage/Storage.hpp"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

// Проверяем создание объекта.
TEST(StorageTests, CreateItem) {
    Storage storage;

    Item item = storage.create("Apple", 10);

    EXPECT_EQ(item.id, 1);
    EXPECT_EQ(item.name, "Apple");
    EXPECT_EQ(item.value, 10);
    EXPECT_EQ(item.version, 1);
}

// Проверяем, что ID увеличивается.
TEST(StorageTests, IDsAreUnique) {
    Storage storage;

    Item first = storage.create("Apple", 10);
    Item second = storage.create("Orange", 20);

    EXPECT_EQ(first.id, 1);
    EXPECT_EQ(second.id, 2);
}

// Проверяем получение существующего объекта.
TEST(StorageTests, GetExistingItem) {
    Storage storage;

    Item created = storage.create("Apple", 10);

    auto result = storage.get(created.id);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "Apple");
    EXPECT_EQ(result->value, 10);
}

// Проверяем получение отсутствующего объекта.
TEST(StorageTests, GetMissingItem) {
    Storage storage;

    auto result = storage.get(100);

    EXPECT_FALSE(result.has_value());
}

// Проверяем список объектов.
TEST(StorageTests, GetAllItems) {
    Storage storage;

    storage.create("Apple", 10);
    storage.create("Orange", 20);
    storage.create("Banana", 30);

    auto items = storage.getAll(0, 100);

    ASSERT_EQ(items.size(), 3);
    EXPECT_EQ(items[0].name, "Apple");
    EXPECT_EQ(items[1].name, "Orange");
    EXPECT_EQ(items[2].name, "Banana");
}

// Проверяем пагинацию.
TEST(StorageTests, Pagination) {
    Storage storage;

    storage.create("Apple", 10);
    storage.create("Orange", 20);
    storage.create("Banana", 30);

    auto items = storage.getAll(1, 1);

    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items[0].name, "Orange");
}

// Проверяем полную замену объекта.
TEST(StorageTests, ReplaceItem) {
    Storage storage;

    Item created = storage.create("Apple", 10);

    auto result = storage.replace(
        created.id, "Orange", 50, std::nullopt
    );

    ASSERT_EQ(result.status, StorageStatus::ok);
    EXPECT_EQ(result.item.name, "Orange");
    EXPECT_EQ(result.item.value, 50);
    EXPECT_EQ(result.item.version, 2);
}

// Проверяем частичное обновление.
TEST(StorageTests, PatchItem) {
    Storage storage;

    Item created = storage.create("Apple", 10);

    auto result = storage.patch(
        created.id,
        std::nullopt,
        50,
        std::nullopt
    );

    ASSERT_EQ(result.status, StorageStatus::ok);
    EXPECT_EQ(result.item.name, "Apple");
    EXPECT_EQ(result.item.value, 50);
    EXPECT_EQ(result.item.version, 2);
}

// Проверяем конфликт версий.
TEST(StorageTests, VersionConflict) {
    Storage storage;

    Item created = storage.create("Apple", 10);

    auto result = storage.replace(
        created.id, "Orange", 20, 999
    );

    EXPECT_EQ(
        result.status,
        StorageStatus::version_conflict
    );

    auto unchanged = storage.get(created.id);

    ASSERT_TRUE(unchanged.has_value());
    EXPECT_EQ(unchanged->name, "Apple");
    EXPECT_EQ(unchanged->value, 10);
}

// Проверяем удаление.
TEST(StorageTests, RemoveItem) {
    Storage storage;

    Item created = storage.create("Apple", 10);

    EXPECT_TRUE(storage.remove(created.id));
    EXPECT_FALSE(storage.get(created.id).has_value());
    EXPECT_FALSE(storage.remove(created.id));
}

// Проверяем увеличение значения.
TEST(StorageTests, IncrementItem) {
    Storage storage;

    Item created = storage.create("Apple", 10);

    auto result = storage.increment(created.id, 5);

    ASSERT_EQ(result.status, StorageStatus::ok);
    EXPECT_EQ(result.item.value, 15);
    EXPECT_EQ(result.item.version, 2);
}

// Проверяем конкурентное увеличение.
TEST(StorageTests, ConcurrentIncrement) {
    Storage storage;

    Item created = storage.create("Counter", 0);

    constexpr int threadCount = 10;
    constexpr int incrementsPerThread = 1000;

    std::vector<std::thread> threads;

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&storage, id = created.id]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                auto result = storage.increment(id, 1);
                EXPECT_EQ(result.status, StorageStatus::ok);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto result = storage.get(created.id);

    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(
        result->value,
        threadCount * incrementsPerThread
    );
}
