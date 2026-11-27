#pragma once

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <atomic>

// Fragmented UTF-8 records: tab-separated fields, LF terminator.
namespace PhoneLink {
constexpr char SERVICE[] = "702d2010-f69e-48e6-ab05-05ece629beb5";
constexpr char INBOX[]   = "702d2011-f69e-48e6-ab05-05ece629beb5";
constexpr char COMMAND[] = "702d2012-f69e-48e6-ab05-05ece629beb5";
constexpr size_t FRAME_SIZE = 256;
struct Frame { char text[FRAME_SIZE]; };

static QueueHandle_t received;
static QueueHandle_t commands;
static std::atomic<bool> connected{false};
static std::atomic<uint32_t> generation{0};
static std::atomic<uint32_t> passkey{0};
static uint32_t showKeyUntil;

class Security : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return passkey.load(); }
  void onPassKeyNotify(uint32_t key) override { passkey.store(key); }
  bool onConfirmPIN(uint32_t) override { return false; }
  bool onSecurityRequest() override { return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t) override {}
};

class Connection : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    generation.fetch_add(1);
    connected.store(true);
  }
  void onDisconnect(BLEServer*) override {
    connected.store(false);
    xQueueReset(received);
    xQueueReset(commands);
    BLEDevice::startAdvertising();
  }
};

class Inbox : public BLECharacteristicCallbacks {
  Frame frame{};
  size_t used = 0;
  uint32_t previousGeneration = 0;
  bool overflow = false;

  void onWrite(BLECharacteristic*, esp_ble_gatts_cb_param_t* event) override {
    uint32_t currentGeneration = generation.load();
    if (currentGeneration != previousGeneration) {
      previousGeneration = currentGeneration;
      used = 0;
      overflow = false;
    }
    for (size_t i = 0; i < event->write.len; ++i) {
      char c = (char)event->write.value[i];
      if (c == '\n') {
        if (used && !overflow) {
          frame.text[used] = '\0';
          if (xQueueSend(received, &frame, 0) != pdPASS) {
            Frame discarded;
            xQueueReceive(received, &discarded, 0);
            xQueueSend(received, &frame, 0);
          }
        }
        used = 0;
        overflow = false;
      } else if (used < FRAME_SIZE - 1) {
        frame.text[used++] = c;
      } else {
        overflow = true;
      }
    }
  }
};

class Command : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* characteristic) override {
    uint8_t action = 0;
    xQueueReceive(commands, &action, 0);
    characteristic->setValue(&action, 1);
  }
};

static bool start() {
  if (received) return true;
  received = xQueueCreate(12, sizeof(Frame));
  commands = xQueueCreate(8, sizeof(uint8_t));
  if (!received || !commands) {
    if (received) vQueueDelete(received);
    if (commands) vQueueDelete(commands);
    received = commands = nullptr;
    return false;
  }
  BLEDevice::init("cuteOS");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
  BLEDevice::setSecurityCallbacks(new Security());
  static BLESecurity security;
  passkey = 100000 + esp_random() % 900000;
  security.setStaticPIN(passkey.load());
  security.setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
  security.setCapability(ESP_IO_CAP_OUT);
  security.setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security.setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  auto server = BLEDevice::createServer();
  server->setCallbacks(new Connection());
  auto service = server->createService(SERVICE);
  auto inbox = service->createCharacteristic(INBOX, BLECharacteristic::PROPERTY_WRITE);
  inbox->setAccessPermissions(ESP_GATT_PERM_WRITE_ENC_MITM);
  inbox->setCallbacks(new Inbox());
  auto command = service->createCharacteristic(COMMAND, BLECharacteristic::PROPERTY_READ);
  command->setAccessPermissions(ESP_GATT_PERM_READ_ENC_MITM);
  command->setCallbacks(new Command());
  service->start();
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE);
  BLEDevice::startAdvertising();
  return true;
}

static void showPairingCode() {
  uint32_t key = 100000 + esp_random() % 900000;
  if (esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &key, sizeof(key)) == ESP_OK) {
    passkey.store(key);
  }
  showKeyUntil = millis() + 120000;
}
}
