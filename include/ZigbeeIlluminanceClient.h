#pragma once

#include <Zigbee.h>

#include <cstdint>

namespace door_display {

    class DoorDisplay;

    class ZigbeeIlluminanceClient final : public ZigbeeEP {
        public:
            ZigbeeIlluminanceClient(uint8_t endpoint, DoorDisplay& display);

            void resolveAndBind();
            bool bindingConfigured() const;
            bool sensorAddressKnown() const;
            void requestCurrentLux();

        protected:
            void zbAttributeRead(uint16_t clusterId, const esp_zb_zcl_attribute_t* attribute,
                                 uint8_t sourceEndpoint, esp_zb_zcl_addr_t sourceAddress) override;

        private:
            enum class SensorBindingStep : uint8_t { Illuminance, Occupancy };

            static void sensorAddressResolved(esp_zb_zdp_status_t status,
                                              esp_zb_zdo_nwk_addr_rsp_t* response,
                                              void* userContext);
            void requestSensorBinding();
            static void bindingComplete(esp_zb_zdp_status_t status, void* userContext);
            void noteSensorReport(SensorBindingStep reportCluster);

            DoorDisplay& display_;
            volatile bool bindingConfigured_ = false;
            volatile uint16_t sensorShortAddress_ = 0xffff;
            SensorBindingStep bindingStep_ = SensorBindingStep::Illuminance;
            esp_zb_ieee_addr_t sensorIeee_ = {};
            bool illuminanceReportObserved_ = false;
            bool occupancyReportObserved_ = false;
            bool missingIeeeReported_ = false;
    };

} // namespace door_display
