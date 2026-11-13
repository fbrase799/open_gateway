#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ogw::sdk {

struct Phase {
    std::int64_t forMs = 0;
    std::string mode;
};

struct Scenario {
    std::int64_t intervalMs = 1000;
    std::uint32_t seed = 1;
    std::vector<Phase> timeline;

    std::string mode_at(std::int64_t elapsed_ms) const {
        if (timeline.empty()) {
            return "normal";
        }
        std::int64_t total = 0;
        for (const auto& phase : timeline) {
            total += phase.forMs;
        }
        if (total <= 0) {
            return timeline.back().mode;
        }
        std::int64_t cursor = elapsed_ms % total;
        if (cursor < 0) {
            cursor += total;
        }
        for (const auto& phase : timeline) {
            if (cursor < phase.forMs) {
                return phase.mode;
            }
            cursor -= phase.forMs;
        }
        return timeline.back().mode;
    }
};

inline double wave(std::uint32_t seed, std::int64_t tick, double center, double amplitude) {
    const double x = 0.35 * static_cast<double>(tick) + static_cast<double>(seed) * 0.01;
    return center + amplitude * std::sin(x);
}

inline Scenario parse_scenario(std::string_view json) {
    class Parser {
    public:
        explicit Parser(std::string_view text) : text_(text) {}

        Scenario parse() {
            Scenario scenario;
            skip();
            if (pos_ >= text_.size()) {
                return scenario;
            }
            expect('{');
            skip();
            while (pos_ < text_.size() && text_[pos_] != '}') {
                const std::string key = parse_string();
                skip();
                expect(':');
                skip();
                if (key == "intervalMs") {
                    scenario.intervalMs = static_cast<std::int64_t>(parse_number());
                } else if (key == "seed") {
                    scenario.seed = static_cast<std::uint32_t>(parse_number());
                } else if (key == "timeline") {
                    scenario.timeline = parse_timeline();
                } else {
                    skip_value();
                }
                skip();
                if (pos_ < text_.size() && text_[pos_] == ',') {
                    ++pos_;
                    skip();
                }
            }
            expect('}');
            if (scenario.intervalMs <= 0) {
                throw std::runtime_error("intervalMs must be positive");
            }
            return scenario;
        }

    private:
        std::string_view text_;
        std::size_t pos_ = 0;

        void skip() {
            while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }

        void expect(char wanted) {
            skip();
            if (pos_ >= text_.size() || text_[pos_] != wanted) {
                throw std::runtime_error("invalid scenario config");
            }
            ++pos_;
        }

        std::string parse_string() {
            skip();
            expect('"');
            std::string value;
            while (pos_ < text_.size() && text_[pos_] != '"') {
                if (text_[pos_] == '\\') {
                    ++pos_;
                    if (pos_ >= text_.size()) {
                        throw std::runtime_error("invalid scenario string");
                    }
                }
                value.push_back(text_[pos_]);
                ++pos_;
            }
            expect('"');
            return value;
        }

        double parse_number() {
            skip();
            const std::size_t start = pos_;
            if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
            if (start == pos_ || (pos_ == start + 1 && (text_[start] == '-' || text_[start] == '+'))) {
                throw std::runtime_error("invalid scenario number");
            }
            return std::stod(std::string(text_.substr(start, pos_ - start)));
        }

        std::vector<Phase> parse_timeline() {
            std::vector<Phase> phases;
            expect('[');
            skip();
            while (pos_ < text_.size() && text_[pos_] != ']') {
                phases.push_back(parse_phase());
                skip();
                if (pos_ < text_.size() && text_[pos_] == ',') {
                    ++pos_;
                    skip();
                }
            }
            expect(']');
            return phases;
        }

        Phase parse_phase() {
            Phase phase;
            expect('{');
            skip();
            while (pos_ < text_.size() && text_[pos_] != '}') {
                const std::string key = parse_string();
                skip();
                expect(':');
                skip();
                if (key == "forMs") {
                    phase.forMs = static_cast<std::int64_t>(parse_number());
                } else if (key == "mode") {
                    phase.mode = parse_string();
                } else {
                    skip_value();
                }
                skip();
                if (pos_ < text_.size() && text_[pos_] == ',') {
                    ++pos_;
                    skip();
                }
            }
            expect('}');
            if (phase.forMs <= 0 || phase.mode.empty()) {
                throw std::runtime_error("timeline phase is invalid");
            }
            return phase;
        }

        void skip_value() {
            skip();
            if (pos_ >= text_.size()) {
                throw std::runtime_error("invalid scenario config");
            }
            if (text_[pos_] == '"') {
                parse_string();
                return;
            }
            if (text_[pos_] == '{') {
                expect('{');
                skip();
                while (pos_ < text_.size() && text_[pos_] != '}') {
                    parse_string();
                    skip();
                    expect(':');
                    skip_value();
                    skip();
                    if (pos_ < text_.size() && text_[pos_] == ',') {
                        ++pos_;
                        skip();
                    }
                }
                expect('}');
                return;
            }
            if (text_[pos_] == '[') {
                expect('[');
                skip();
                while (pos_ < text_.size() && text_[pos_] != ']') {
                    skip_value();
                    skip();
                    if (pos_ < text_.size() && text_[pos_] == ',') {
                        ++pos_;
                        skip();
                    }
                }
                expect(']');
                return;
            }
            parse_number();
        }
    };

    return Parser(json).parse();
}

}  // namespace ogw::sdk
