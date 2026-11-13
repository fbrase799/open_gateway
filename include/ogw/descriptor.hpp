#pragma once

#include "ogw/registry.hpp"

#include <string>
#include <string_view>

namespace ogw {

struct ParsedDraft {
    bool ok = false;
    std::string error;
    DeviceDraft draft;
};

ParsedDraft parse_device_json(std::string_view json, bool require_full);

}  // namespace ogw
