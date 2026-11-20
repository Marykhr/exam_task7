
#include "Router.hpp"

#include <nlohmann/json.hpp>

#include <charconv>
#include <string_view>

using json = nlohmann::json;

Router::Router(Storage& storage)
    : storage_(storage) {}

std::string Router::handle(const HttpRequest& request) {
    std::string path = request.path;
    json response;

    std::size_t query = path.find('?');

    if (query != std::string::npos) {
        path.resize(query);
    }

    auto sendResponse = [&](int code,
                            const std::string& message,
                            const json& body) {
        std::string bodyText = body.dump();

        std::string result =
            "HTTP/1.1 " + std::to_string(code) + " " + message + "\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " + std::to_string(bodyText.size()) + "\r\n";

        result += request.keepAlive
            ? "Connection: keep-alive\r\n\r\n"
            : "Connection: close\r\n\r\n";

        result += bodyText;
        return result;
    };

    if (request.method == "GET" && path == "/health") {
        return sendResponse(
            200, "OK", json{{"status", "ok"}}
        );
    }

    if (request.method == "POST" && path == "/items") {
        try {
            json body = json::parse(request.body);

            if (!body.is_object() ||
                !body.contains("name") ||
                !body.contains("value") ||
                !body["name"].is_string() ||
                !body["value"].is_number_integer()) {
                return sendResponse(
                    400, "Bad Request",
                    json{{"error", "Invalid item data"}}
                );
            }

            Item item = storage_.create(
                body["name"].get<std::string>(),
                body["value"].get<std::int64_t>()
            );

            return sendResponse(
                201, "Created",
                json{
                    {"id", item.id},
                    {"name", item.name},
                    {"value", item.value},
                    {"version", item.version}
                }
            );

        } catch (const json::exception&) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid JSON"}}
            );
        }
    }

    if (request.method == "GET" && path == "/items") {
        std::size_t offset = 0;
        std::size_t limit = 100;

        auto parseParameter = [](const std::string& url,
                                 const std::string& name,
                                 std::size_t& value) {
            std::size_t queryPos = url.find('?');

            if (queryPos == std::string::npos) {
                return true;
            }

            std::string_view params(
                url.data() + queryPos + 1,
                url.size() - queryPos - 1
            );

            while (!params.empty()) {
                std::size_t amp = params.find('&');

                std::string_view part = params.substr(0, amp);
                std::size_t equal = part.find('=');

                if (equal != std::string_view::npos &&
                    part.substr(0, equal) == name) {

                    auto number = part.substr(equal + 1);

                    if (number.empty()) {
                        return false;
                    }

                    std::size_t parsed = 0;

                    auto result = std::from_chars(
                        number.data(),
                        number.data() + number.size(),
                        parsed
                    );

                    if (result.ec != std::errc{} ||
                        result.ptr != number.data() + number.size()) {
                        return false;
                    }

                    value = parsed;
                }

                if (amp == std::string::npos) {
                    break;
                }

                params.remove_prefix(amp + 1);
            }

            return true;
        };

        if (!parseParameter(request.path, "offset", offset) ||
            !parseParameter(request.path, "limit", limit) ||
            limit == 0) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid pagination parameters"}}
            );
        }

        auto items = storage_.getAll(offset, limit);

        response = json::array();

        for (const Item& item : items) {
            response.push_back({
                {"id", item.id},
                {"name", item.name},
                {"value", item.value},
                {"version", item.version}
            });
        }

        return sendResponse(200, "OK", response);
    }


    if (request.method == "PUT" &&
        path.rfind("/items/", 0) == 0) {

        std::string idString = path.substr(7);
        std::uint64_t id = 0;

        auto idResult = std::from_chars(
            idString.data(),
            idString.data() + idString.size(),
            id
        );

        if (idString.empty() ||
            idResult.ec != std::errc{} ||
            idResult.ptr != idString.data() + idString.size()) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid item ID"}}
            );
        }

        try {
            json body = json::parse(request.body);

            if (!body.is_object() ||
                !body.contains("name") ||
                !body.contains("value") ||
                !body["name"].is_string() ||
                !body["value"].is_number_integer()) {
                return sendResponse(
                    400, "Bad Request",
                    json{{"error", "Invalid item data"}}
                );
            }

            std::optional<std::uint64_t> expectedVersion;

            if (body.contains("expectedVersion")) {
                if (!body["expectedVersion"].is_number_unsigned() &&
                    (!body["expectedVersion"].is_number_integer() ||
                     body["expectedVersion"].get<std::int64_t>() < 0)) {
                    return sendResponse(
                        400, "Bad Request",
                        json{{"error", "Invalid expectedVersion"}}
                    );
                }

                expectedVersion =
                    body["expectedVersion"].get<std::uint64_t>();
            }

            auto result = storage_.replace(
                id,
                body["name"].get<std::string>(),
                body["value"].get<std::int64_t>(),
                expectedVersion
            );

            if (result.status == StorageStatus::not_found) {
                return sendResponse(
                    404, "Not Found",
                    json{{"error", "Item not found"}}
                );
            }

            if (result.status == StorageStatus::version_conflict) {
                return sendResponse(
                    409, "Conflict",
                    json{{"error", "Version conflict"}}
                );
            }

            return sendResponse(
                200, "OK",
                json{
                    {"id", result.item.id},
                    {"name", result.item.name},
                    {"value", result.item.value},
                    {"version", result.item.version}
                }
            );

        } catch (const json::exception&) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid JSON"}}
            );
        }
    }

    if (request.method == "PATCH" &&
        path.rfind("/items/", 0) == 0) {

        std::string idString = path.substr(7);
        std::uint64_t id = 0;

        auto idResult = std::from_chars(
            idString.data(),
            idString.data() + idString.size(),
            id
        );

        if (idString.empty() ||
            idResult.ec != std::errc{} ||
            idResult.ptr != idString.data() + idString.size()) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid item ID"}}
            );
        }

        try {
            json body = json::parse(request.body);

            if (!body.is_object() ||
                body.empty()) {
                return sendResponse(
                    400, "Bad Request",
                    json{{"error", "Invalid item data"}}
                );
            }

            std::optional<std::string> name;
            std::optional<std::int64_t> value;
            std::optional<std::uint64_t> expectedVersion;

            if (body.contains("name")) {
                if (!body["name"].is_string()) {
                    return sendResponse(
                        400, "Bad Request",
                        json{{"error", "Invalid name"}}
                    );
                }
                name = body["name"].get<std::string>();
            }

            if (body.contains("value")) {
                if (!body["value"].is_number_integer()) {
                    return sendResponse(
                        400, "Bad Request",
                        json{{"error", "Invalid value"}}
                    );
                }
                value = body["value"].get<std::int64_t>();
            }

            if (body.contains("expectedVersion")) {
                if (!body["expectedVersion"].is_number_unsigned() &&
                    (!body["expectedVersion"].is_number_integer() ||
                     body["expectedVersion"].get<std::int64_t>() < 0)) {
                    return sendResponse(
                        400, "Bad Request",
                        json{{"error", "Invalid expectedVersion"}}
                    );
                }

                expectedVersion =
                    body["expectedVersion"].get<std::uint64_t>();
            }

            if (!name && !value) {
                return sendResponse(
                    400, "Bad Request",
                    json{{"error", "No fields to update"}}
                );
            }

            auto result = storage_.patch(
                id, name, value, expectedVersion
            );

            if (result.status == StorageStatus::not_found) {
                return sendResponse(
                    404, "Not Found",
                    json{{"error", "Item not found"}}
                );
            }

            if (result.status == StorageStatus::version_conflict) {
                return sendResponse(
                    409, "Conflict",
                    json{{"error", "Version conflict"}}
                );
            }

            return sendResponse(
                200, "OK",
                json{
                    {"id", result.item.id},
                    {"name", result.item.name},
                    {"value", result.item.value},
                    {"version", result.item.version}
                }
            );

        } catch (const json::exception&) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid JSON"}}
            );
        }
    }

    if (request.method == "DELETE" &&
        path.rfind("/items/", 0) == 0) {

        std::string idString = path.substr(7);
        std::uint64_t id = 0;

        auto idResult = std::from_chars(
            idString.data(),
            idString.data() + idString.size(),
            id
        );

        if (idString.empty() ||
            idResult.ec != std::errc{} ||
            idResult.ptr != idString.data() + idString.size()) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid item ID"}}
            );
        }

        if (!storage_.remove(id)) {
            return sendResponse(
                404, "Not Found",
                json{{"error", "Item not found"}}
            );
        }

        return sendResponse(
            200, "OK",
            json{{"message", "Item deleted"}}
        );
    }

    if (request.method == "POST" &&
        path.size() >= 18 &&
        path.compare(path.size() - 10, 10, "/increment") == 0) {

        std::string idString = path.substr(
            7, path.size() - 17
        );

        std::uint64_t id = 0;

        auto idResult = std::from_chars(
            idString.data(),
            idString.data() + idString.size(),
            id
        );

        if (idString.empty() ||
            idResult.ec != std::errc{} ||
            idResult.ptr != idString.data() + idString.size()) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid item ID"}}
            );
        }

        try {
            json body = json::parse(request.body);

            if (!body.is_object() ||
                !body.contains("delta") ||
                !body["delta"].is_number_integer()) {
                return sendResponse(
                    400, "Bad Request",
                    json{{"error", "Invalid delta"}}
                );
            }

            auto result = storage_.increment(
                id,
                body["delta"].get<std::int64_t>()
            );

            if (result.status == StorageStatus::not_found) {
                return sendResponse(
                    404, "Not Found",
                    json{{"error", "Item not found"}}
                );
            }

            if (result.status == StorageStatus::overflow) {
                return sendResponse(
                    400, "Bad Request",
                    json{{"error", "Value overflow"}}
                );
            }

            return sendResponse(
                200, "OK",
                json{
                    {"id", result.item.id},
                    {"name", result.item.name},
                    {"value", result.item.value},
                    {"version", result.item.version}
                }
            );

        } catch (const json::exception&) {
            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid JSON"}}
            );
        }
    }



    if (request.method == "GET" &&
        path.rfind("/items/", 0) == 0) {

        std::string idString = path.substr(7);

        std::uint64_t id = 0;

        auto result = std::from_chars(
            idString.data(),
            idString.data() + idString.size(),
            id
        );

        if (idString.empty() ||
            result.ec != std::errc{} ||
            result.ptr != idString.data() + idString.size()) {

            return sendResponse(
                400, "Bad Request",
                json{{"error", "Invalid item ID"}}
            );
        }

        auto item = storage_.get(id);

        if (!item.has_value()) {
            return sendResponse(
                404, "Not Found",
                json{{"error", "Item not found"}}
            );
        }

        return sendResponse(
            200, "OK",
            json{
                {"id", item->id},
                {"name", item->name},
                {"value", item->value},
                {"version", item->version}
            }
        );
    }


    return sendResponse(
        404, "Not Found",
        json{{"error", "Route not found"}}
    );
}
