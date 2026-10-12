#include "DoorDisplay.h"
#include "Configuration.h"

#include <Arduino.h>

#include <cmath>

namespace door_display {

    DoorDisplay::DoorDisplay(uint8_t primaryLedPin, uint8_t secondaryLedPin, uint8_t ledCount)
        : primaryPixels_(ledCount, primaryLedPin, NEO_GRB + NEO_KHZ800),
          secondaryPixels_(ledCount, secondaryLedPin, NEO_GRB + NEO_KHZ800) {}

    void DoorDisplay::begin() {
        primaryPixels_.begin();
        secondaryPixels_.begin();
        primaryPixels_.clear();
        secondaryPixels_.clear();
        showState(State::Joining, statusSweepPixel(millis()));
    }

    void DoorDisplay::recordIlluminance(uint16_t rawValue) {
        const uint16_t darkThreshold = luxToZcl(DOOR_DARK_LUX);
        const uint16_t lightThreshold = luxToZcl(DOOR_LIGHT_LUX);

        portENTER_CRITICAL(&stateMux_);
        if (!haveLux_) {
            isLight_ = rawValue >= lightThreshold;
        } else if (rawValue <= darkThreshold) {
            isLight_ = false;
        } else if (rawValue >= lightThreshold) {
            isLight_ = true;
        }
        haveLux_ = true;
        lastReportMs_ = millis();
        portEXIT_CRITICAL(&stateMux_);
    }

    void DoorDisplay::recordOccupancy(bool detected) {
        portENTER_CRITICAL(&stateMux_);
        motionDetected_ = detected;
        portEXIT_CRITICAL(&stateMux_);
    }

    bool DoorDisplay::hasIlluminance() const {
        portENTER_CRITICAL(&stateMux_);
        const bool hasIlluminance = haveLux_;
        portEXIT_CRITICAL(&stateMux_);
        return hasIlluminance;
    }

    void DoorDisplay::update(bool zigbeeConnected) {
        bool haveLux;
        bool isLight;
        bool motionDetected;
        uint32_t lastReportMs;
        portENTER_CRITICAL(&stateMux_);
        haveLux = haveLux_;
        isLight = isLight_;
        motionDetected = motionDetected_;
        lastReportMs = lastReportMs_;
        portEXIT_CRITICAL(&stateMux_);

        State state = State::Joining;
        if (zigbeeConnected) {
            const bool stale = !haveLux || (millis() - lastReportMs > DOOR_STALE_SECONDS * 1000UL);
            // Lux is authoritative. Motion can show yellow only when the last lux
            // state is not light; it never changes isLight or the lux timestamp.
            state = (haveLux && isLight) ? State::Light
                    : motionDetected     ? State::Motion
                    : stale              ? State::Stale
                                         : State::Dark;
        }

        const uint8_t statusPixel = statusSweepPixel(millis());
        const bool isStatusState = state == State::Joining || state == State::Stale;
        if (state != previousState_ || (isStatusState && statusPixel != previousStatusPixel_)) {
            showState(state, statusPixel);
            previousState_ = state;
            previousStatusPixel_ = statusPixel;
        }
    }

    uint16_t DoorDisplay::luxToZcl(unsigned lux) {
        if (lux == 0) {
            return 0;
        }
        return static_cast<uint16_t>(1.0 + 10000.0 * log10(static_cast<double>(lux)) + 0.5);
    }

    uint8_t DoorDisplay::statusSweepPixel(uint32_t now) const {
        const uint16_t ledCount = primaryPixels_.numPixels();
        if (ledCount < 2) {
            return 0;
        }

        const uint32_t fullCycleMs = 2U * kStatusSweepHalfCycleMs;
        uint32_t elapsedMs = now % fullCycleMs;
        if (elapsedMs > kStatusSweepHalfCycleMs) {
            elapsedMs = fullCycleMs - elapsedMs;
        }
        return static_cast<uint8_t>(elapsedMs * (ledCount - 1U) / kStatusSweepHalfCycleMs);
    }

    void DoorDisplay::showState(State state, uint8_t statusPixel) {
        primaryPixels_.clear();
        secondaryPixels_.clear();

        uint32_t color = 0;
        bool fillStrip = false;
        switch (state) {
            case State::Joining:
                // Pinging the Zigbee network sweeps a single amber status pixel.
                color = primaryPixels_.Color(255, 96, 0);
                break;
            case State::Stale:
                // A missing lux report sweeps a single magenta fault pixel.
                color = primaryPixels_.Color(255, 0, 255);
                break;
            case State::Dark:
                color = primaryPixels_.Color(0, 0, 12);
                fillStrip = true;
                break;
            case State::Light:
                color = primaryPixels_.Color(255, 0, 0);
                fillStrip = true;
                break;
            case State::Motion:
                color = primaryPixels_.Color(255, 255, 0);
                fillStrip = true;
                break;
        }

        if (fillStrip) {
            primaryPixels_.fill(color);
            secondaryPixels_.fill(color);
        } else {
            primaryPixels_.setPixelColor(statusPixel, color);
            secondaryPixels_.setPixelColor(statusPixel, color);
        }
        primaryPixels_.show();
        secondaryPixels_.show();
    }

} // namespace door_display
