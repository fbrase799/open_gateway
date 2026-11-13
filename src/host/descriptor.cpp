#include "ogw/descriptor.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>

namespace ogw {
namespace {

ParsedDraft fail(std::string error) {
    ParsedDraft parsed;
    parsed.error = std::move(error);
    return parsed;
}

std::optional<std::string> optional_string(const nlohmann::json& object, const char* key) {
    if (!object.contains(key) || object.at(key).is_null()) {
        return std::nullopt;
    }
    if (!object.at(key).is_string()) {
        throw std::runtime_error(std::string(key) + " must be a string");
    }
    return object.at(key).get<std::string>();
}

}  // namespace

ParsedDraft parse_device_json(std::string_view json, bool require_full) {
    nlohmann::json document;
    try {
        document = nlohmann::json::parse(json.begin(), json.end());
    } catch (const std::exception&) {
        return fail("device document is not valid json");
    }
    try {
    if (!document.is_object()) {
        return fail("device document must be an object");
    }

    ParsedDraft parsed;
    if (!document.contains("localId") || !document.at("localId").is_string()) {
        return fail("localId is required");
    }
    parsed.draft.localId = document.at("localId").get<std::string>();

    if (document.contains("displayName")) {
        if (!document.at("displayName").is_string()) {
            return fail("displayName must be a string");
        }
        parsed.draft.displayName = document.at("displayName").get<std::string>();
    } else if (require_full) {
        return fail("displayName is required");
    }

    if (document.contains("status")) {
        if (!document.at("status").is_string()) {
            return fail("status must be a string");
        }
        auto status = parse_device_status(document.at("status").get<std::string>());
        if (!status) {
            return fail("status is invalid");
        }
        parsed.draft.status = *status;
    }

    if (document.contains("capabilities")) {
        if (!document.at("capabilities").is_array()) {
            return fail("capabilities must be an array");
        }
        std::vector<Capability> capabilities;
        for (const auto& item : document.at("capabilities")) {
            if (!item.is_object()) {
                return fail("capability must be an object");
            }
            Capability capability;
            if (!item.contains("id") || !item.at("id").is_string()) {
                return fail("capability id is required");
            }
            capability.id = item.at("id").get<std::string>();
            if (!item.contains("kind") || !item.at("kind").is_string()) {
                return fail("capability kind is required");
            }
            capability.kind = item.at("kind").get<std::string>();
            if (!item.contains("valueType") || !item.at("valueType").is_string()) {
                return fail("capability valueType is required");
            }
            auto value_type = parse_value_type(item.at("valueType").get<std::string>());
            if (!value_type) {
                return fail("capability valueType is invalid");
            }
            capability.valueType = *value_type;
            if (!item.contains("displayName") || !item.at("displayName").is_string()) {
                return fail("capability displayName is required");
            }
            capability.displayName = item.at("displayName").get<std::string>();
            capability.unit = optional_string(item, "unit");
            if (auto presentation = optional_string(item, "presentation")) {
                capability.presentation = *presentation;
            }
            if (!item.contains("staleAfterMs") || !item.at("staleAfterMs").is_number_integer()) {
                return fail("capability staleAfterMs is required");
            }
            capability.staleAfterMs = item.at("staleAfterMs").get<std::int64_t>();
            if (item.contains("range")) {
                if (!item.at("range").is_object() || !item.at("range").contains("min") || !item.at("range").contains("max") ||
                    !item.at("range").at("min").is_number() || !item.at("range").at("max").is_number()) {
                    return fail("range must contain numeric min and max");
                }
                capability.rangeMin = item.at("range").at("min").get<double>();
                capability.rangeMax = item.at("range").at("max").get<double>();
            }
            if (item.contains("enumStates")) {
                if (!item.at("enumStates").is_array()) {
                    return fail("enumStates must be an array");
                }
                for (const auto& state : item.at("enumStates")) {
                    if (!state.is_object() || !state.contains("id") || !state.at("id").is_string() ||
                        !state.contains("label") || !state.at("label").is_string()) {
                        return fail("enum state requires id and label");
                    }
                    EnumState enum_state;
                    enum_state.id = state.at("id").get<std::string>();
                    enum_state.label = state.at("label").get<std::string>();
                    enum_state.tone = "neutral";
                    if (state.contains("tone")) {
                        if (!state.at("tone").is_string()) {
                            return fail("enum tone must be a string");
                        }
                        enum_state.tone = state.at("tone").get<std::string>();
                    }
                    capability.enumStates.push_back(std::move(enum_state));
                }
            }
            capabilities.push_back(std::move(capability));
        }
        parsed.draft.capabilities = std::move(capabilities);
    } else if (require_full) {
        return fail("capabilities are required");
    }

    parsed.ok = true;
    return parsed;
    } catch (const std::exception& ex) {
        return fail(ex.what());
    }
}

}  // namespace ogw
