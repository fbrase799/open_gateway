#include "ogw/application.hpp"
#include "ogw/config.hpp"
#include "ogw/log.hpp"

#include <exception>

int main() {
    try {
        auto config = ogw::load_config();
        ogw::set_log_level(config.logLevel);
        ogw::Application app(std::move(config));
        return app.run(false);
    } catch (const std::exception& ex) {
        ogw::log_message(ogw::LogLevel::Error, "main", ex.what());
        return 1;
    }
}
