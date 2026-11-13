#include "can_decode.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("showcase status frame decodes encoder, duty, and direction") {
    CanFrameMap frame;
    frame.id = 0x101;
    frame.signals.push_back({"encoder_counts", "number", "counts", 0, 4, 1, true});
    frame.signals.push_back({"duty", "number", "%", 4, 1, 1, false});
    frame.signals.push_back({"direction", "number", "", 5, 1, 1, false});
    const std::uint8_t data[] = {0xFE, 0xFF, 0xFF, 0xFF, 40, 1, 0, 0};
    const auto decoded = decode_frame(frame, data, sizeof(data));
    REQUIRE(decoded.size() == 3);
    REQUIRE(decoded[0].capabilityId == "encoder_counts");
    REQUIRE(decoded[0].value == -2);
    REQUIRE(decoded[1].value == 40);
    REQUIRE(decoded[2].value == 1);
}

TEST_CASE("a short CAN frame yields no signal") {
    CanSignal signal{"speed", "number", "rpm", 0, 2, 1};
    const std::uint8_t data[] = {0x01};
    REQUIRE_FALSE(decode_signal(data, sizeof(data), signal).has_value());
}
