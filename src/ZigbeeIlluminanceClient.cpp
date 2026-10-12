#include "ZigbeeIlluminanceClient.h"

#include "Configuration.h"
#include "DoorDisplay.h"

#include <Arduino.h>

#include <cmath>
#include <cstring>

namespace door_display {

    ZigbeeIlluminanceClient::ZigbeeIlluminanceClient(uint8_t endpoint, DoorDisplay& display)
        : ZigbeeEP(endpoint), display_(display) {
        _device_id = ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID;

        esp_zb_basic_cluster_cfg_t basicConfig = {};
        basicConfig.zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
        basicConfig.power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE;

        esp_zb_identify_cluster_cfg_t identifyConfig = {};
        identifyConfig.identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE;

        _cluster_list = esp_zb_zcl_cluster_list_create();
        esp_zb_cluster_list_add_basic_cluster(_cluster_list,
                                              esp_zb_basic_cluster_create(&basicConfig),
                                              ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_identify_cluster(_cluster_list,
                                                 esp_zb_identify_cluster_create(&identifyConfig),
                                                 ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
        esp_zb_cluster_list_add_illuminance_meas_cluster(
            _cluster_list,
            esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT),
            ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
        esp_zb_cluster_list_add_occupancy_sensing_cluster(
            _cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING),
            ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);

        _ep_config.endpoint = endpoint;
        _ep_config.app_profile_id = ESP_ZB_AF_HA_PROFILE_ID;
        _ep_config.app_device_id = _device_id;
        _ep_config.app_device_version = 0;
    }

    void ZigbeeIlluminanceClient::resolveAndBind() {
        if (bindingConfigured_) {
            return;
        }
        if (kSensorIeee == 0) {
            if (!missingIeeeReported_) {
                Serial.println("Set DOOR_SENSOR_IEEE before automatic binding can start");
                missingIeeeReported_ = true;
            }
            return;
        }

        esp_zb_zdo_nwk_addr_req_param_t request = {};
        request.dst_nwk_addr = 0x0000; // Zigbee coordinator
        for (size_t i = 0; i < sizeof(request.ieee_addr_of_interest); ++i) {
            request.ieee_addr_of_interest[i] = static_cast<uint8_t>(kSensorIeee >> (8 * i));
        }

        if (!acquireCommandLock()) {
            return;
        }
        esp_zb_zdo_nwk_addr_req(&request, ZigbeeIlluminanceClient::sensorAddressResolved, this);
        releaseCommandLock();
    }

    bool ZigbeeIlluminanceClient::bindingConfigured() const {
        return bindingConfigured_;
    }

    bool ZigbeeIlluminanceClient::sensorAddressKnown() const {
        return sensorShortAddress_ != 0xffff;
    }

    void ZigbeeIlluminanceClient::requestCurrentLux() {
        const uint16_t sensorAddress = sensorShortAddress_;
        if (sensorAddress == 0xffff) {
            return;
        }

        esp_zb_zcl_read_attr_cmd_t request = {};
        request.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        request.zcl_basic_cmd.src_endpoint = _endpoint;
        request.zcl_basic_cmd.dst_endpoint = 1;
        request.zcl_basic_cmd.dst_addr_u.addr_short = sensorAddress;
        request.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT;

        uint16_t attribute = ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID;
        request.attr_number = 1;
        request.attr_field = &attribute;

        Serial.printf("Requesting current lux from 0x%04x\n", sensorAddress);
        if (!readClusterAttribute(&request)) {
            Serial.println("Could not queue current-lux request");
        }
    }

    void ZigbeeIlluminanceClient::zbAttributeRead(uint16_t clusterId,
                                                  const esp_zb_zcl_attribute_t* attribute,
                                                  uint8_t sourceEndpoint,
                                                  esp_zb_zcl_addr_t sourceAddress) {
        if (clusterId == ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING && attribute != nullptr &&
            attribute->id == ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID &&
            attribute->data.value != nullptr) {
            uint8_t occupancy = 0;
            memcpy(&occupancy, attribute->data.value, sizeof(occupancy));
            const bool detected = (occupancy & 0x01U) != 0;

            display_.recordOccupancy(detected);
            noteSensorReport(SensorBindingStep::Occupancy);

            if (sourceAddress.addr_type == ESP_ZB_ZCL_ADDR_TYPE_SHORT) {
                Serial.printf("Motion %s from 0x%04x endpoint %u\n",
                              detected ? "detected" : "clear", sourceAddress.u.short_addr,
                              sourceEndpoint);
            } else {
                Serial.printf("Motion %s from endpoint %u\n", detected ? "detected" : "clear",
                              sourceEndpoint);
            }
            return;
        }

        if (clusterId != ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT || attribute == nullptr ||
            attribute->id != ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID ||
            attribute->data.type != ESP_ZB_ZCL_ATTR_TYPE_U16 || attribute->data.value == nullptr) {
            return;
        }

        uint16_t raw = 0;
        memcpy(&raw, attribute->data.value, sizeof(raw));
        if (raw == 0xffff) {
            return;
        }

        display_.recordIlluminance(raw);
        noteSensorReport(SensorBindingStep::Illuminance);

        const double lux = raw == 0 ? 0.0 : pow(10.0, (static_cast<double>(raw) - 1.0) / 10000.0);
        if (sourceAddress.addr_type == ESP_ZB_ZCL_ADDR_TYPE_SHORT) {
            Serial.printf("Lux report %.1f lx (raw %u) from 0x%04x endpoint %u\n", lux, raw,
                          sourceAddress.u.short_addr, sourceEndpoint);
        } else {
            Serial.printf("Lux report %.1f lx (raw %u) from endpoint %u\n", lux, raw,
                          sourceEndpoint);
        }
    }

    void ZigbeeIlluminanceClient::sensorAddressResolved(esp_zb_zdp_status_t status,
                                                        esp_zb_zdo_nwk_addr_rsp_t* response,
                                                        void* userContext) {
        auto* instance = static_cast<ZigbeeIlluminanceClient*>(userContext);
        if (instance == nullptr || instance->bindingConfigured_) {
            return;
        }
        if (status != ESP_ZB_ZDP_STATUS_SUCCESS || response == nullptr) {
            Serial.printf("Coordinator could not resolve sensor IEEE (ZDO status %u)\n", status);
            return;
        }
        instance->sensorShortAddress_ = response->nwk_addr;
        memcpy(instance->sensorIeee_, response->ieee_addr, sizeof(instance->sensorIeee_));
        instance->requestSensorBinding();
    }

    void ZigbeeIlluminanceClient::requestSensorBinding() {
        esp_zb_zdo_bind_req_param_t bindRequest = {};
        bindRequest.req_dst_addr = sensorShortAddress_;
        memcpy(bindRequest.src_address, sensorIeee_, sizeof(bindRequest.src_address));
        bindRequest.src_endp = 1;
        bindRequest.cluster_id = bindingStep_ == SensorBindingStep::Illuminance
                                     ? ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT
                                     : ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING;
        bindRequest.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
        esp_zb_get_long_address(bindRequest.dst_address_u.addr_long);
        bindRequest.dst_endp = _endpoint;

        const char* clusterName =
            bindingStep_ == SensorBindingStep::Illuminance ? "illuminance" : "motion";
        Serial.printf("Sensor is at 0x%04x; requesting its %s bind to display endpoint %u\n",
                      sensorShortAddress_, clusterName, _endpoint);
        esp_zb_zdo_device_bind_req(&bindRequest, ZigbeeIlluminanceClient::bindingComplete, this);
    }

    void ZigbeeIlluminanceClient::bindingComplete(esp_zb_zdp_status_t status, void* userContext) {
        auto* instance = static_cast<ZigbeeIlluminanceClient*>(userContext);
        if (instance == nullptr || instance->bindingConfigured_) {
            return;
        }
        if (status == ESP_ZB_ZDP_STATUS_SUCCESS) {
            if (instance->bindingStep_ == SensorBindingStep::Illuminance) {
                Serial.println("Illuminance binding accepted by sensor");
                instance->bindingStep_ = SensorBindingStep::Occupancy;
                instance->requestSensorBinding();
            } else {
                instance->bindingConfigured_ = true;
                Serial.println("Motion binding accepted by sensor; bindings ready");
            }
        } else {
            const char* clusterName =
                instance->bindingStep_ == SensorBindingStep::Illuminance ? "Illuminance" : "Motion";
            Serial.printf("%s binding rejected or timed out (ZDO status %u)\n", clusterName,
                          status);
        }
    }

    void ZigbeeIlluminanceClient::noteSensorReport(SensorBindingStep reportCluster) {
        if (reportCluster == SensorBindingStep::Illuminance) {
            illuminanceReportObserved_ = true;
            // The report confirms the illuminance binding is already usable, even
            // if the sleepy sensor did not return a ZDO Bind response.
            if (bindingStep_ == SensorBindingStep::Illuminance) {
                bindingStep_ = SensorBindingStep::Occupancy;
            }
        } else {
            occupancyReportObserved_ = true;
        }

        if (illuminanceReportObserved_ && occupancyReportObserved_ && !bindingConfigured_) {
            bindingConfigured_ = true;
            Serial.println("Observed illuminance and motion reports; bindings ready");
        }
    }

} // namespace door_display
