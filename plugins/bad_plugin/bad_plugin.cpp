#include "ogw/sdk/plugin.hpp"

#include <stdexcept>

namespace {

class BadPlugin final : public ogw::sdk::Plugin {
public:
    ogw::sdk::Info info() const override { return {"bad-plugin", "Bad Plugin", "0.0.1"}; }

    void init(std::string_view) override { throw std::runtime_error("invalid plugin configuration"); }

    void start() override {}
    void stop() override {}
};

}  // namespace

OGW_EXPORT_PLUGIN(BadPlugin)
