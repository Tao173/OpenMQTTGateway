#ifdef ESP32
#  include "User_config.h"
#  ifdef ZgatewayBT
#    include "ArduinoJson.h"
#    include "ArduinoLog.h"
#    include "ZgatewayBLEConnect.h"
#    define convertTemp_CtoF(c) ((c * 1.8) + 32)

extern std::vector<BLEdevice*> devices;

NimBLERemoteCharacteristic* zBLEConnect::getCharacteristic(const NimBLEUUID& service,
                                                           const NimBLEUUID& characteristic) {
  BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
  if (!m_pClient) {
    Log.error(F("No BLE client" CR));
  } else if (!m_pClient->isConnected() && !m_pClient->connect()) {
    Log.error(F("Connect to: %s failed" CR), m_pClient->getPeerAddress().toString().c_str());
  } else {
    BLERemoteService* pRemoteService = m_pClient->getService(service);
    if (!pRemoteService) {
      Log.notice(F("Failed to find service UUID: %s" CR), service.toString().c_str());
    } else {
      Log.trace(F("Found service: %s" CR), service.toString().c_str());
      Log.trace(F("Client isConnected, freeHeap: %d" CR), ESP.getFreeHeap());
      pRemoteCharacteristic = pRemoteService->getCharacteristic(characteristic);
      if (!pRemoteCharacteristic) {
        Log.notice(F("Failed to find characteristic UUID: %s" CR), characteristic.toString().c_str());
      }
    }
  }

  return pRemoteCharacteristic;
}

bool zBLEConnect::writeData(BLEAction* action) {
  NimBLERemoteCharacteristic* pChar = getCharacteristic(action->service, action->characteristic);
  if (pChar && (pChar->canWrite() || pChar->canWriteNoResponse())) {
    switch (action->value_type) {
      case BLE_VAL_HEX: {
        int len = action->value.length();
        if (len % 2) {
          Log.error(F("Invalid HEX value length" CR));
          return false;
        }

        std::vector<uint8_t> buf;
        buf.reserve(len / 2);
        for (auto i = 0; i < len; i += 2) {
          char sVal[3] = {action->value[i], action->value[i + 1], 0};
          buf.push_back((uint8_t)strtoul(sVal, nullptr, 16));
        }
        return pChar->writeValue(&buf[0], buf.size(), !pChar->canWriteNoResponse());
      }
      case BLE_VAL_INT:
        return pChar->writeValue(strtol(action->value.c_str(), nullptr, 0), !pChar->canWriteNoResponse());
      case BLE_VAL_FLOAT:
        return pChar->writeValue(strtod(action->value.c_str(), nullptr), !pChar->canWriteNoResponse());
      default:
        return pChar->writeValue(action->value, !pChar->canWriteNoResponse());
    }
  }
  return false;
}

bool zBLEConnect::readData(BLEAction* action) {
  NimBLERemoteCharacteristic* pChar = getCharacteristic(action->service, action->characteristic);

  if (pChar && pChar->canRead()) {
    action->value = pChar->readValue();
    if (action->value != "") {
      return true;
    }
  }
  return false;
}

bool zBLEConnect::processActions(std::vector<BLEAction>& actions) {
  bool result = false;
  if (actions.size() > 0) {
    for (auto& it : actions) {
      if (NimBLEAddress(it.addr) == m_pClient->getPeerAddress()) {
        DynamicJsonDocument BLEdataBuffer(JSON_MSG_BUFFER);
        JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
        BLEdata["id"] = m_pClient->getPeerAddress().toString().c_str();
        BLEdata["service"] = it.service.toString();
        BLEdata["characteristic"] = it.characteristic.toString();

        if (it.write) {
          Log.trace(F("processing BLE write" CR));
          BLEdata["write"] = std::string(it.value);
          result = writeData(&it);
        } else {
          Log.trace(F("processing BLE read" CR));
          result = readData(&it);
          if (result) {
            switch (it.value_type) {
              case BLE_VAL_HEX: {
                BLEdata["read"] = NimBLEUtils::dataToHexString(it.value.data(), it.value.size());
                break;
              }
              case BLE_VAL_INT: {
                int ival = *(int*)it.value.data();
                BLEdata["read"] = ival;
                break;
              }
              case BLE_VAL_FLOAT: {
                float fval = *(double*)it.value.data();
                BLEdata["read"] = fval;
                break;
              }
              default:
                BLEdata["read"] = it.value.c_str();
                break;
            }
          }
        }

        it.complete = result;
        BLEdata["success"] = result;
        if (result || it.ttl <= 1) {
          buildTopicFromId(BLEdata, subjectBTtoMQTT);
          enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
        }
      }
    }
  }

  return result;
}

/*-----------------------LYWSD03MMC && MHO_C401 HANDLING-----------------------*/
void LYWSD03MMC_connect::notifyCB(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (m_taskHandle == nullptr) {
    return; // unexpected notification
  }
  if (!BTProcessLock) {
    Log.trace(F("Callback from %s characteristic" CR), pChar->getUUID().toString().c_str());

    if (length == 5) {
      Log.trace(F("Device identified creating BLE buffer" CR));
      DynamicJsonDocument BLEdataBuffer(JSON_MSG_BUFFER);
      JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
      auto mac_addr = m_pClient->getPeerAddress().toString().c_str();
      for (std::vector<BLEdevice*>::iterator it = devices.begin(); it != devices.end(); ++it) {
        BLEdevice* p = *it;
        if ((strcmp(p->macAdr, mac_addr) == 0)) {
          if (p->sensorModel_id == BLEconectable::id::LYWSD03MMC)
            BLEdata["model"] = "LYWSD03MMC";
          else if (p->sensorModel_id == BLEconectable::id::MHO_C401)
            BLEdata["model"] = "MHO-C401";
        }
      }
      BLEdata["id"] = mac_addr;
      Log.trace(F("Device identified in CB: %s" CR), mac_addr);
      BLEdata["tempc"] = (float)((pData[0] | (pData[1] << 8)) * 0.01);
      BLEdata["tempf"] = (float)(convertTemp_CtoF((pData[0] | (pData[1] << 8)) * 0.01));
      BLEdata["hum"] = (float)(pData[2]);
      BLEdata["volt"] = (float)(((pData[4] * 256) + pData[3]) / 1000.0);
      BLEdata["batt"] = (float)(((((pData[4] * 256) + pData[3]) / 1000.0) - 2.1) * 100);
      buildTopicFromId(BLEdata, subjectBTtoMQTT);
      enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
    } else {
      Log.notice(F("Invalid notification data" CR));
      return;
    }
  } else {
    Log.trace(F("Callback process canceled by BTProcessLock" CR));
  }

  xTaskNotifyGive(m_taskHandle);
}

void LYWSD03MMC_connect::publishData() {
  NimBLEUUID serviceUUID("ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6");
  NimBLEUUID charUUID("ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6");
  NimBLERemoteCharacteristic* pChar = getCharacteristic(serviceUUID, charUUID);

  if (pChar && pChar->canNotify()) {
    Log.trace(F("Registering notification" CR));
    if (pChar->subscribe(true, std::bind(&LYWSD03MMC_connect::notifyCB, this,
                                         std::placeholders::_1, std::placeholders::_2,
                                         std::placeholders::_3, std::placeholders::_4))) {
      m_taskHandle = xTaskGetCurrentTaskHandle();
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLE_CNCT_TIMEOUT)) == pdFALSE) {
        m_taskHandle = nullptr;
      }
    } else {
      Log.notice(F("Failed registering notification" CR));
    }
  }
}

/*-----------------------DT24 HANDLING-----------------------*/
void DT24_connect::notifyCB(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (m_taskHandle == nullptr) {
    return; // unexpected notification
  }

  if (!BTProcessLock) {
    Log.trace(F("Callback from %s characteristic" CR), pChar->getUUID().toString().c_str());
    if (length == 20) {
      m_data.assign(pData, pData + length);
      return;
    } else if (m_data.size() == 20 && length == 16) {
      m_data.insert(m_data.end(), pData, pData + length);

      // DT24-BLE data format
      // https://github.com/NiceLabs/atorch-console/blob/master/docs/protocol-design.md#dc-meter-report
      // Data comes as two packets ( 20 and 16 ), and am only processing first
      Log.trace(F("Device identified creating BLE buffer" CR));
      DynamicJsonDocument BLEdataBuffer(JSON_MSG_BUFFER);
      JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
      auto mac_address = m_pClient->getPeerAddress().toString().c_str();
      BLEdata["model"] = "DT24";
      BLEdata["id"] = mac_address;
      Log.trace(F("Device identified in CB: %s" CR), mac_address);
      BLEdata["volt"] = (float)(((m_data[4] * 256 * 256) + (m_data[5] * 256) + m_data[6]) / 10.0);
      BLEdata["current"] = (float)(((m_data[7] * 256 * 256) + (m_data[8] * 256) + m_data[9]) / 1000.0);
      BLEdata["power"] = (float)(((m_data[10] * 256 * 256) + (m_data[11] * 256) + m_data[12]) / 10.0);
      BLEdata["energy"] = (float)(((m_data[13] * 256 * 256 * 256) + (m_data[14] * 256 * 256) + (m_data[15] * 256) + m_data[16]) / 100.0);
      BLEdata["price"] = (float)(((m_data[17] * 256 * 256) + (m_data[18] * 256) + m_data[19]) / 100.0);
      BLEdata["tempc"] = (float)(m_data[24] * 256) + m_data[25];
      BLEdata["tempf"] = (float)(convertTemp_CtoF((m_data[24] * 256) + m_data[25]));
      buildTopicFromId(BLEdata, subjectBTtoMQTT);
      enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
    } else {
      Log.notice(F("Invalid notification data" CR));
      return;
    }
  } else {
    Log.trace(F("Callback process canceled by BTProcessLock" CR));
  }

  xTaskNotifyGive(m_taskHandle);
}

void DT24_connect::publishData() {
  NimBLEUUID serviceUUID("ffe0");
  NimBLEUUID charUUID("ffe1");
  NimBLERemoteCharacteristic* pChar = getCharacteristic(serviceUUID, charUUID);

  if (pChar && pChar->canNotify()) {
    Log.trace(F("Registering notification" CR));
    if (pChar->subscribe(true, std::bind(&DT24_connect::notifyCB, this,
                                         std::placeholders::_1, std::placeholders::_2,
                                         std::placeholders::_3, std::placeholders::_4))) {
      m_taskHandle = xTaskGetCurrentTaskHandle();
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLE_CNCT_TIMEOUT)) == pdFALSE) {
        m_taskHandle = nullptr;
      }
    } else {
      Log.notice(F("Failed registering notification" CR));
    }
  }
}

/*-----------------------BM2 HANDLING-----------------------*/
void BM2_connect::notifyCB(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (m_taskHandle == nullptr) {
    return; // unexpected notification
  }

  if (!BTProcessLock) {
    Log.trace(F("Callback from %s characteristic" CR), pChar->getUUID().toString().c_str());
    if (length == 16) {
      Log.trace(F("Device identified creating BLE buffer" CR));
      DynamicJsonDocument BLEdataBuffer(JSON_MSG_BUFFER);
      JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
      BLEdata["model"] = "BM2 Battery Monitor";
      BLEdata["id"] = m_pClient->getPeerAddress().toString().c_str();
      mbedtls_aes_context aes;
      mbedtls_aes_init(&aes);
      unsigned char output[16];
      unsigned char iv[16] = {};
      unsigned char key[16] = {
          108,
          101,
          97,
          103,
          101,
          110,
          100,
          255,
          254,
          49,
          56,
          56,
          50,
          52,
          54,
          54,
      };
      mbedtls_aes_setkey_dec(&aes, key, 128);
      mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, 16, iv, (uint8_t*)&pData[0], output);
      mbedtls_aes_free(&aes);
      float volt = ((output[2] | (output[1] << 8)) >> 4) / 100.0f;
      BLEdata["volt"] = volt;
      Log.trace(F("volt: %F" CR), volt);
      // to avoid the BM2 device tracker going offline because of the voltage MQTT message without an RSSI value
      BLEdata["rssi"] = -60;
      buildTopicFromId(BLEdata, subjectBTtoMQTT);
      enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
    } else {
      Log.notice(F("Invalid notification data" CR));
      return;
    }
  } else {
    Log.trace(F("Callback process canceled by BTProcessLock" CR));
  }

  xTaskNotifyGive(m_taskHandle);
}

void BM2_connect::publishData() {
  NimBLEUUID serviceUUID("fff0");
  NimBLEUUID charUUID("fff4");
  NimBLERemoteCharacteristic* pChar = getCharacteristic(serviceUUID, charUUID);

  if (pChar && pChar->canNotify()) {
    Log.trace(F("Registering notification" CR));
    if (pChar->subscribe(true, std::bind(&BM2_connect::notifyCB, this,
                                         std::placeholders::_1, std::placeholders::_2,
                                         std::placeholders::_3, std::placeholders::_4))) {
      m_taskHandle = xTaskGetCurrentTaskHandle();
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLE_CNCT_TIMEOUT)) == pdFALSE) {
        m_taskHandle = nullptr;
      }
    } else {
      Log.notice(F("Failed registering notification" CR));
    }
  }
}

/*-----------------------HHCCJCY01HHCC HANDLING-----------------------*/
void HHCCJCY01HHCC_connect::publishData() {
  NimBLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
  NimBLEUUID charUUID("00001a00-0000-1000-8000-00805f9b34fb");
  NimBLEUUID charUUID2("00001a02-0000-1000-8000-00805f9b34fb");
  NimBLERemoteCharacteristic* pChar = getCharacteristic(serviceUUID, charUUID);

  if (pChar) {
    Log.trace(F("Read mode" CR));
    uint8_t buf[2] = {0xA0, 0x1F};
    pChar->writeValue(buf, 2, true);
    int batteryValue = -1;
    NimBLERemoteCharacteristic* pChar2 = getCharacteristic(serviceUUID, charUUID2);
    if (pChar2) {
      std::string value;
      value = pChar2->readValue();
      const char* val2 = value.c_str();
      batteryValue = val2[0];
      DynamicJsonDocument BLEdataBuffer(JSON_MSG_BUFFER);
      JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
      BLEdata["model"] = "HHCCJCY01HHCC";
      BLEdata["id"] = m_pClient->getPeerAddress().toString().c_str();
      BLEdata["batt"] = (int)batteryValue;
      buildTopicFromId(BLEdata, subjectBTtoMQTT);
      enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
    } else {
      Log.notice(F("Failed getting characteristic" CR));
    }
  } else {
    Log.notice(F("Failed getting characteristic" CR));
  }
}

/*-----------------------XMWSDJ04MMC HANDLING-----------------------*/
void XMWSDJ04MMC_connect::notifyCB(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (m_taskHandle == nullptr) {
    return; // unexpected notification
  }
  if (!BTProcessLock) {
    Log.trace(F("Callback from %s characteristic" CR), pChar->getUUID().toString().c_str());

    if (length == 6) {
      Log.trace(F("Device identified creating BLE buffer" CR));
      DynamicJsonDocument BLEdataBuffer(JSON_MSG_BUFFER);
      JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
      auto mac_address = m_pClient->getPeerAddress().toString().c_str();
      BLEdata["model"] = "XMWSDJ04MMC";
      BLEdata["id"] = mac_address;
      Log.trace(F("Device identified in CB: %s" CR), mac_address);
      BLEdata["tempc"] = (float)((pData[0] | (pData[1] << 8)) * 0.1);
      BLEdata["tempf"] = (float)(convertTemp_CtoF((pData[0] | (pData[1] << 8)) * 0.1));
      BLEdata["hum"] = (float)((pData[2] | (pData[3] << 8)) * 0.1);
      BLEdata["volt"] = (float)((pData[4] | (pData[5] << 8)) / 1000.0);
      BLEdata["batt"] = (float)((((pData[4] | (pData[5] << 8)) / 1000.0) - 2.1) * 100);
      buildTopicFromId(BLEdata, subjectBTtoMQTT);
      enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
    } else {
      Log.notice(F("Invalid notification data" CR));
      return;
    }
  } else {
    Log.trace(F("Callback process canceled by BTProcessLock" CR));
  }

  xTaskNotifyGive(m_taskHandle);
}

void XMWSDJ04MMC_connect::publishData() {
  NimBLEUUID serviceUUID("ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6");
  NimBLEUUID charUUID("ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6");
  NimBLERemoteCharacteristic* pChar = getCharacteristic(serviceUUID, charUUID);

  if (pChar && pChar->canNotify()) {
    Log.trace(F("Registering notification" CR));
    if (pChar->subscribe(true, std::bind(&XMWSDJ04MMC_connect::notifyCB, this,
                                         std::placeholders::_1, std::placeholders::_2,
                                         std::placeholders::_3, std::placeholders::_4))) {
      m_taskHandle = xTaskGetCurrentTaskHandle();
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLE_CNCT_TIMEOUT)) == pdFALSE) {
        m_taskHandle = nullptr;
      }
    } else {
      Log.notice(F("Failed registering notification" CR));
    }
  }
}

/*-----------------------SBS1 HANDLING-----------------------*/
void SBS1_connect::notifyCB(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (m_taskHandle == nullptr) {
    return; // unexpected notification
  }
  if (!BTProcessLock) {
    Log.trace(F("Callback from %s characteristic" CR), pChar->getUUID().toString().c_str());

    if (length) {
      m_notifyVal = *pData;
    } else {
      Log.notice(F("Invalid notification data" CR));
      return;
    }
  } else {
    Log.trace(F("Callback process canceled by BTProcessLock" CR));
  }

  xTaskNotifyGive(m_taskHandle);
}

bool SBS1_connect::processActions(std::vector<BLEAction>& actions) {
  NimBLEUUID serviceUUID("cba20d00-224d-11e6-9fb8-0002a5d5c51b");
  NimBLEUUID charUUID("cba20002-224d-11e6-9fb8-0002a5d5c51b");
  NimBLEUUID notifyCharUUID("cba20003-224d-11e6-9fb8-0002a5d5c51b");
  static byte ON[] = {0x57, 0x01, 0x01};
  static byte OFF[] = {0x57, 0x01, 0x02};
  static byte PRESS[] = {0x57, 0x01, 0x00};
  static byte DOWN[] = {0x57, 0x01, 0x03};
  static byte UP[] = {0x57, 0x01, 0x04};

  bool result = false;
  if (actions.size() > 0) {
    for (auto& it : actions) {
      if (it.addr == m_pClient->getPeerAddress()) {
        NimBLERemoteCharacteristic* pChar = getCharacteristic(serviceUUID, charUUID);
        NimBLERemoteCharacteristic* pNotifyChar = getCharacteristic(serviceUUID, notifyCharUUID);

        if (it.write && pChar && pNotifyChar) {
          Log.trace(F("processing Switchbot %s" CR), it.value.c_str());
          if (pNotifyChar->subscribe(true,
                                     std::bind(&SBS1_connect::notifyCB,
                                               this, std::placeholders::_1, std::placeholders::_2,
                                               std::placeholders::_3, std::placeholders::_4),
                                     true)) {
            if (it.value == "on") {
              result = pChar->writeValue(ON, 3, false);
            } else if (it.value == "off") {
              result = pChar->writeValue(OFF, 3, false);
            } else if (it.value == "press") {
              result = pChar->writeValue(PRESS, 3, false);
            } else if (it.value == "down") {
              result = pChar->writeValue(DOWN, 3, false);
            } else if (it.value == "up") {
              result = pChar->writeValue(UP, 3, false);
            }

            if (result) {
              m_taskHandle = xTaskGetCurrentTaskHandle();
              if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLE_CNCT_TIMEOUT)) == pdFALSE) {
                m_taskHandle = nullptr;
              }
              result = m_notifyVal == 0x01;
            }
          }
        }

        it.complete = result;
        if (result || it.ttl <= 1) {
          StaticJsonDocument<JSON_MSG_BUFFER> BLEdataBuffer;
          JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
          BLEdata["id"] = m_pClient->getPeerAddress().toString().c_str();
          BLEdata["state"] = std::string(it.value);
          buildTopicFromId(BLEdata, subjectBTtoMQTT);
          enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
        }
      }
    }
  }

  return result;
}

/*-----------------------SBBT HANDLING-----------------------*/
void SBBT_connect::notifyCB(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (m_taskHandle == nullptr) {
    return; // unexpected notification
  }
  if (!BTProcessLock) {
    Log.trace(F("Callback from %s characteristic" CR), pChar->getUUID().toString().c_str());

    if (length) {
      m_notifyVal = *pData;
    } else {
      Log.notice(F("Invalid notification data" CR));
      return;
    }
  } else {
    Log.trace(F("Callback process canceled by BTProcessLock" CR));
  }

  xTaskNotifyGive(m_taskHandle);
}

bool SBBT_connect::processActions(std::vector<BLEAction>& actions) {
  NimBLEUUID serviceUUID("cba20d00-224d-11e6-9fb8-0002a5d5c51b");
  NimBLEUUID charUUID("cba20002-224d-11e6-9fb8-0002a5d5c51b");
  NimBLEUUID notifyCharUUID("cba20003-224d-11e6-9fb8-0002a5d5c51b");
  static byte OPEN[] = {0x57, 0x0f, 0x45, 0x01, 0x01, 0x01, 0x32};
  static byte CLOSE_DOWN[] = {0x57, 0x0f, 0x45, 0x01, 0x01, 0x01, 0x00};
  static byte CLOSE_UP[] = {0x57, 0x0f, 0x45, 0x01, 0x01, 0x01, 0x64};
  static byte MOVE[] = {0x57, 0x0f, 0x45, 0x01, 0x01, 0x01, 0x00};
  static byte STOP[] = {0x57, 0x0f, 0x45, 0x01, 0x00, 0x01};

  bool result = false;
  if (actions.size() > 0) {
    for (auto& it : actions) {
      if (NimBLEAddress(it.addr) == m_pClient->getPeerAddress()) {
        NimBLERemoteCharacteristic* pChar = getCharacteristic(serviceUUID, charUUID);
        NimBLERemoteCharacteristic* pNotifyChar = getCharacteristic(serviceUUID, notifyCharUUID);
        int value = -99;
        if (it.value_type == BLE_VAL_INT) {
          value = std::stoi(it.value);
        }
        if (it.write && pChar && pNotifyChar) {
          Log.trace(F("processing Switchbot %s" CR), it.value.c_str());
          if (pNotifyChar->subscribe(true,
                                     std::bind(&SBBT_connect::notifyCB,
                                               this, std::placeholders::_1, std::placeholders::_2,
                                               std::placeholders::_3, std::placeholders::_4),
                                     true)) {
            if (it.value == "open" && it.value_type == BLE_VAL_STRING) {
              result = pChar->writeValue(OPEN, 7, false);
              value = 50;
            } else if (it.value == "close_down" && it.value_type == BLE_VAL_STRING) {
              result = pChar->writeValue(CLOSE_DOWN, 7, false);
              value = 0;
            } else if (it.value == "close_up" && it.value_type == BLE_VAL_STRING) {
              result = pChar->writeValue(CLOSE_UP, 7, false);
              value = 100;
            } else if (it.value == "stop" && it.value_type == BLE_VAL_STRING) {
              result = pChar->writeValue(STOP, 6, false);
            } else if (it.value_type == BLE_VAL_INT) {
              if (value >= 0 && value <= 100) {
                byte posByte = (byte)value;
                MOVE[6] = posByte;
                result = pChar->writeValue(MOVE, 7, false);
              }
            } else if (value == -1 && it.value_type == BLE_VAL_INT) {
              result = pChar->writeValue(STOP, 6, false);
            }

            if (result) {
              m_taskHandle = xTaskGetCurrentTaskHandle();
              if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLE_CNCT_TIMEOUT)) == pdFALSE) {
                m_taskHandle = nullptr;
              }
              result = m_notifyVal == 0x01;
            }
          }
        }

        it.complete = result;
        if (result || it.ttl <= 1) {
          StaticJsonDocument<JSON_MSG_BUFFER> BLEdataBuffer;
          JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
          BLEdata["id"] = it.addr.toString().c_str();
          if (value != -99 || value != -1)
            BLEdata["tilt"] = value;
          if (value == 50) {
            BLEdata["open"] = 100;
            BLEdata["direction"] = "-";
          } else if (value < 50 && value >= 0) {
            BLEdata["open"] = value * 2;
            BLEdata["direction"] = "down";
          } else if (value > 50 && value <= 100) {
            BLEdata["open"] = (100 - value) * 2;
            BLEdata["direction"] = "up";
          }
          buildTopicFromId(BLEdata, subjectBTtoMQTT);
          enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
        }
      }
    }
  }

  return result;
}

/*-----------------------SBCU HANDLING-----------------------*/
void SBCU_connect::notifyCB(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  if (m_taskHandle == nullptr) {
    return; // unexpected notification
  }
  if (!BTProcessLock) {
    Log.trace(F("Callback from %s characteristic" CR), pChar->getUUID().toString().c_str());

    if (length) {
      m_notifyVal = *pData;
    } else {
      Log.notice(F("Invalid notification data" CR));
      return;
    }
  } else {
    Log.trace(F("Callback process canceled by BTProcessLock" CR));
  }

  xTaskNotifyGive(m_taskHandle);
}

bool SBCU_connect::processActions(std::vector<BLEAction>& actions) {
  NimBLEUUID serviceUUID("cba20d00-224d-11e6-9fb8-0002a5d5c51b");
  NimBLEUUID charUUID("cba20002-224d-11e6-9fb8-0002a5d5c51b");
  NimBLEUUID notifyCharUUID("cba20003-224d-11e6-9fb8-0002a5d5c51b");
  static byte CLOSE[] = {0x57, 0x0f, 0x45, 0x01, 0x01, 0x01, 0x64};
  static byte OPEN[] = {0x57, 0x0f, 0x45, 0x01, 0x01, 0x01, 0x00};
  static byte MOVE[] = {0x57, 0x0f, 0x45, 0x01, 0x01, 0x01, 0x00};
  static byte STOP[] = {0x57, 0x0f, 0x45, 0x01, 0x00, 0x01};

  bool result = false;
  if (actions.size() > 0) {
    for (auto& it : actions) {
      if (NimBLEAddress(it.addr) == m_pClient->getPeerAddress()) {
        NimBLERemoteCharacteristic* pChar = getCharacteristic(serviceUUID, charUUID);
        NimBLERemoteCharacteristic* pNotifyChar = getCharacteristic(serviceUUID, notifyCharUUID);
        int value = -99;
        if (it.value_type == BLE_VAL_INT) {
          value = std::stoi(it.value);
        }
        if (it.write && pChar && pNotifyChar) {
          Log.trace(F("processing Switchbot %s" CR), it.value.c_str());
          if (pNotifyChar->subscribe(true,
                                     std::bind(&SBCU_connect::notifyCB,
                                               this, std::placeholders::_1, std::placeholders::_2,
                                               std::placeholders::_3, std::placeholders::_4),
                                     true)) {
            if (it.value == "open" && it.value_type == BLE_VAL_STRING) {
              result = pChar->writeValue(OPEN, 7, false);
              value = 100;
            } else if (it.value == "close" && it.value_type == BLE_VAL_STRING) {
              result = pChar->writeValue(CLOSE, 7, false);
              value = 0;
            } else if (it.value == "stop" && it.value_type == BLE_VAL_STRING) {
              result = pChar->writeValue(STOP, 6, false);
            } else if (it.value_type == BLE_VAL_INT) {
              if (value >= 0 && value <= 100) {
                byte posByte = (byte)value;
                MOVE[6] = posByte;
                result = pChar->writeValue(MOVE, 7, false);
              }
            } else if (value == -1 && it.value_type == BLE_VAL_INT) {
              result = pChar->writeValue(STOP, 6, false);
            }

            if (result) {
              m_taskHandle = xTaskGetCurrentTaskHandle();
              if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BLE_CNCT_TIMEOUT)) == pdFALSE) {
                m_taskHandle = nullptr;
              }
              result = m_notifyVal == 0x01;
            }
          }
        }

        it.complete = result;
        if (result || it.ttl <= 1) {
          StaticJsonDocument<JSON_MSG_BUFFER> BLEdataBuffer;
          JsonObject BLEdata = BLEdataBuffer.to<JsonObject>();
          BLEdata["id"] = it.addr.toString().c_str();
          if (value != -99 || value != -1)
            BLEdata["position"] = value;
          buildTopicFromId(BLEdata, subjectBTtoMQTT);
          enqueueJsonObject(BLEdata, QueueSemaphoreTimeOutTask);
        }
      }
    }
  }

  return result;
}

#  endif //ZgatewayBT
#endif //ESP32
