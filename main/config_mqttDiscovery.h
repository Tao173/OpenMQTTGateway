/*
  OpenMQTTGateway  - ESP8266 or Arduino program for home automation

   Act as a gateway between your 433mhz, infrared IR, BLE, LoRa signal and one interface like an MQTT broker
   Send and receiving command by MQTT

   This files enables to set your parameter for the Home assistant mqtt Discovery

    Copyright: (c) Rafal Herok

    This file is part of OpenMQTTGateway.

    OpenMQTTGateway is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenMQTTGateway is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#ifndef config_mqttDiscovery_h
#define config_mqttDiscovery_h

extern String getUniqueId(String name, String sufix);
extern void pubMqttDiscovery();

/**
 * Create a discover messages form a list of attribute
 * 
 * @param mac the MAC address
 * @param sensorList[][0] = component type
 * @param sensorList[][1] = name
 * @param sensorList[][2] = availability topic
 * @param sensorList[][3] = device class
 * @param sensorList[][4] = value template
 * @param sensorList[][5] = payload on
 * @param sensorList[][6] = payload off
 * @param sensorList[][7] = unit of measurement
 * @param sensorList[][8] = unit of measurement
 * @param sensorCount number of sensor
 * @param device_name name of sensors
 * @param device_manufacturer name of manufacturer
 * @param device_model the model
 * */
extern void createDiscoveryFromList(const char* mac,
                                    const char* sensorList[][9],
                                    int sensorCount,
                                    const char* device_name,
                                    const char* device_manufacturer,
                                    const char* device_model);

/**
 * @brief Generate message and publish it on an MQTT discovery exploiter. For HA @see https://www.home-assistant.io/docs/mqtt/discovery/
 * 
 * @param sensor_type the Type
 * @param st_topic set state topic,
 * @param s_name set name,
 * @param unique_id set uniqueId
 * @param availability_topic set availability_topic,
 * @param device_class set device_class,
 * @param value_template set value_template,
 * @param payload_on set payload_on,
 * @param payload_off set payload_off,
 * @param unit_of_meas set unit_of_meas,
 * @param off_delay set off_delay
 * @param payload_available set payload_available,
 * @param payload_not_available set payload_not_available
 * @param gateway_entity set is a gateway entity, 
 * @param cmd_topic set command topic
 * @param device_name set device name, 
 * @param device_manufacturer set device manufacturer, 
 * @param device_model set device model, 
 * @param device_mac set device MAC, 
 * @param retainCmd set retain
 * @param state_class set state class
 * 
 * */
extern void createDiscovery(const char* sensor_type,
                            const char* state_topic, const char* s_name, const char* unique_id,
                            const char* availability_topic, const char* device_class, const char* value_template,
                            const char* payload_on, const char* payload_off, const char* unit_of_meas,
                            int off_delay,
                            const char* payload_available, const char* payload_not_available, bool gateway_entity, const char* command_topic,
                            const char* device_name, const char* device_manufacturer, const char* device_model, const char* device_mac, bool retainCmd,
                            const char* state_class, const char* state_off = nullptr, const char* state_on = nullptr, const char* enum_options = nullptr, const char* command_template = nullptr);

/**
 * @brief Create a message for Discovery Device Trigger. For HA @see https://www.home-assistant.io/integrations/device_trigger.mqtt/
 * @param topic                 Mandatory - The MQTT topic subscribed to receive trigger events.
 * @param type                  The type of the trigger, e.g. button_short_press. Entries supported by the HA Frontend: button_short_press, button_short_release, button_long_press, button_long_release, button_double_press, button_triple_press, button_quadruple_press, button_quintuple_press. If set to an unsupported value, will render as subtype type, e.g. button_1 spammed with type set to spammed and subtype set to button_1
 * @param subtype               The subtype of the trigger, e.g. button_1. Entries supported by the HA frontend: turn_on, turn_off, button_1, button_2, button_3, button_4, button_5, button_6. If set to an unsupported value, will render as subtype type, e.g. left_button pressed with type set to button_short_press and subtype set to left_button
  * @param object_id             The object_id of the trigger.   
 * @param value_template        The template to render the value of the trigger. The template can use the variables trigger.id, trigger.type, trigger.subtype, trigger.payload, trigger.payload_json, trigger.topic, trigger.timestamp, trigger.value, trigger.value_json. The template can be a string or a JSON object. If the template is a JSON object, it must be a valid JSON object. If the template is a string, it will be rendered as a string. If the template is a JSON object, it will be rendered as a JSON object.
 */
void announceGatewayTrigger(const char* topic,
                            const char* type,
                            const char* subtype,
                            const char* object_id,
                            const char* value_template);

#ifdef discovery_Topic //Deprecated - use discovery_Prefix instead
#  pragma message("compiler directive discovery_Topic is deprecated, use discovery_Prefix instead")
#  define discovery_Prefix discovery_Topic
#endif
#ifndef discovery_Prefix
#  define discovery_Prefix "homeassistant"
#endif
char discovery_prefix[parameters_size + 1] = discovery_Prefix;

// discovery_republish_on_reconnect false to publish discovery topics over MQTT only with first connect
// discovery_republish_on_reconnect true to always republish discovery topics over MQTT when connection is re-established
#ifndef discovery_republish_on_reconnect
#  define discovery_republish_on_reconnect false
#endif

#ifndef DiscoveryAutoOffTimer
#  define DiscoveryAutoOffTimer 1800000 // Timer (ms) that trigger auto discovery to off after activation, the goal is to avoid the discovery of entities that are not expected
#endif

#ifndef GATEWAY_MANUFACTURER
#  define GATEWAY_MANUFACTURER "OMG_community"
#endif

#ifndef ForceDeviceName
#  define ForceDeviceName false // Set to true to force the device name to be from the name of the device and not the model
#endif

/*-------------- Auto discovery macros-----------------*/
// Home assistant autodiscovery value key definition
#define jsonBatt        "{{ value_json.batt | is_defined }}"
#define jsonLux         "{{ value_json.lux | is_defined }}"
#define jsonPres        "{{ value_json.pres | is_defined }}"
#define jsonFer         "{{ value_json.fer | is_defined }}"
#define jsonFor         "{{ value_json.for | is_defined }}"
#define jsonMoi         "{{ value_json.moi | is_defined }}"
#define jsonHum         "{{ value_json.hum | is_defined }}"
#define jsonStep        "{{ value_json.steps | is_defined }}"
#define jsonWeight      "{{ value_json.weight | is_defined }}"
#define jsonPresence    "{{ value_json.presence | is_defined }}"
#define jsonAltim       "{{ value_json.altim | is_defined }}"
#define jsonAltif       "{{ value_json.altift | is_defined }}"
#define jsonTempc       "{{ value_json.tempc | is_defined }}"
#define jsonTempc2      "{{ value_json.tempc2 | is_defined }}"
#define jsonTempc3      "{{ value_json.tempc3 | is_defined }}"
#define jsonTempc4      "{{ value_json.tempc4 | is_defined }}"
#define jsonTempf       "{{ value_json.tempf | is_defined }}"
#define jsonMsg         "{{ value_json.message | is_defined }}"
#define jsonVal         "{{ value_json.value | is_defined }}"
#define jsonVolt        "{{ value_json.volt | is_defined }}"
#define jsonCurrent     "{{ value_json.current | is_defined }}"
#define jsonPower       "{{ value_json.power | is_defined }}"
#define jsonEnergy      "{{ value_json.energy | is_defined }}"
#define jsonGpio        "{{ value_json.gpio | is_defined }}"
#define jsonFtcd        "{{ value_json.ftcd | is_defined }}"
#define jsonWm2         "{{ value_json.wattsm2 | is_defined }}"
#define jsonAdc         "{{ value_json.adc | is_defined }}"
#define jsonPa          "{{ float(value_json.pa) * 0.01 | is_defined }}"
#define jsonId          "{{ value_json.id | is_defined }}"
#define jsonAddress     "{{ value_json.address | is_defined }}"
#define jsonOpen        "{{ value_json.open | is_defined }}"
#define jsonTime        "{{ value_json.time | is_defined }}"
#define jsonCount       "{{ value_json.count | is_defined }}"
#define jsonAlarm       "{{ value_json.alarm | is_defined }}"
#define jsonInuse       "{{ value_json.power | is_defined | float > 0 }}"
#define jsonInuseRN8209 "{% if value_json.power > 0.02 -%} on {% else %} off {%- endif %}"
#define jsonVoltBM2     "{% if value_json.uuid is not defined and value_json.volt is defined -%} {{value_json.volt}} {%- endif %}"

#define stateClassNone            ""
#define stateClassMeasurement     "measurement"
#define stateClassTotal           "total"
#define stateClassTotalIncreasing "total_increasing"

// From https://github.com/home-assistant/core/blob/d7ac4bd65379e11461c7ce0893d3533d8d8b8cbf/homeassistant/const.py#L225
// List of classes available in Home Assistant
const char* availableHASSClasses[] = {"battery_charging",
                                      "battery",
                                      "carbon_dioxide",
                                      "carbon_monoxide",
                                      "current",
                                      "data_size",
                                      "distance",
                                      "door",
                                      "duration",
                                      "energy",
                                      "enum",
                                      "gas",
                                      "humidity",
                                      "illuminance",
                                      "irradiance",
                                      "lock",
                                      "motion",
                                      "moving",
                                      "pm1",
                                      "pm10",
                                      "pm25",
                                      "power_factor",
                                      "power",
                                      "precipitation_intensity",
                                      "precipitation",
                                      "pressure",
                                      "problem",
                                      "restart",
                                      "signal_strength",
                                      "sound_pressure",
                                      "temperature",
                                      "timestamp",
                                      "voltage",
                                      "water",
                                      "weight",
                                      "wind_speed",
                                      "window"};

// From https://github.com/home-assistant/core/blob/d7ac4bd65379e11461c7ce0893d3533d8d8b8cbf/homeassistant/const.py#L379
// List of units available in Home Assistant
const char* availableHASSUnits[] = {"W",
                                    "kW",
                                    "V",
                                    "kWh",
                                    "A",
                                    "W",
                                    "°C",
                                    "°F",
                                    "ms",
                                    "s",
                                    "min",
                                    "hPa",
                                    "L",
                                    "kg",
                                    "lb",
                                    "µS/cm",
                                    "ppm",
                                    "μg/m³",
                                    "m³",
                                    "mg/m³",
                                    "m/s²",
                                    "mV",
                                    "lx",
                                    "Ω",
                                    "%",
                                    "bar",
                                    "bpm",
                                    "dB",
                                    "dBm",
                                    "B",
                                    "UV index",
                                    "m/s",
                                    "km/h",
                                    "°",
                                    "mm",
                                    "mm/h",
                                    "cm"};

// Define the command used to update through OTA depending if we want to update from dev nightly or latest release
#if DEVELOPMENTOTA
#  define LATEST_OR_DEV "{\"version\":\"dev\"}"
#else
#  define LATEST_OR_DEV "{\"version\":\"latest\"}"
#endif
#endif
