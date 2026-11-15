#pragma once

#include <optional>
#include <string_view>

namespace ogw {

inline bool simulated_source_id(std::string_view id) {
    constexpr std::string_view suffix = "-sim";
    return id.size() >= suffix.size() && id.substr(id.size() - suffix.size()) == suffix;
}

inline bool resolve_simulated(std::string_view id, std::optional<bool> configured) {
    return configured.value_or(simulated_source_id(id));
}

struct SourceRequest {
    bool simulated = false;
    bool requested = false;
};

inline bool probe_source(const SourceRequest& source) { return source.requested && !source.simulated; }

inline bool use_simulated_source(const SourceRequest&, bool) { return false; }

}  // namespace ogw
