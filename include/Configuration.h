#pragma once

#include <cstdint>

#ifndef ZIGBEE_MODE_ED
#error "This firmware must be built in Zigbee end-device mode"
#endif

// Set this to the SNZB-03PR2's IEEE/EUI-64, written in the usual order, for
// example 0x00124B0012345678ULL. Its short address is intentionally not kept:
// Zigbee can change that address whenever the sensor rejoins.
#ifndef DOOR_SENSOR_IEEE
#define DOOR_SENSOR_IEEE 0ULL
#endif

namespace door_display {

    constexpr uint8_t kDisplayEndpoint = 10;
    // Both outputs carry the same data so one firmware supports either display wiring.
    constexpr uint8_t kLedPrimaryPin = 0;
    constexpr uint8_t kLedSecondaryPin = 14;
    constexpr uint8_t kLedCount = 5;
    constexpr uint64_t kSensorIeee = static_cast<uint64_t>(DOOR_SENSOR_IEEE);
    constexpr uint32_t kBindingSearchIntervalMs = 10'000;
    constexpr uint32_t kLuxReadIntervalMs = 10'000;
    constexpr uint32_t kLoopDelayMs = 50;
    constexpr uint32_t kStartupAnimationDurationMs = 1'000;
    constexpr uint32_t kStatusSweepHalfCycleMs = 600;

} // namespace door_display
