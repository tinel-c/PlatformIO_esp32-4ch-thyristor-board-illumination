#include <Arduino.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "board_pins.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy include/secrets.h.example to include/secrets.h"
#endif

namespace {

constexpr char kFirmwareVersion[] = "0.1.0";
constexpr char kClientId[] = "PlatformIO_esp32-4ch-thyristor-board-illumination";
constexpr char kStatusTopic[] = "illumination/4ch-thyristor/status";
constexpr char kCommandTopic[] = "illumination/4ch-thyristor/command/set";
constexpr uint32_t kStatusIntervalMs = 30000;
constexpr uint32_t kWifiRetryMs = 5000;

constexpr int kChannelPins[4] = {PIN_CH1, PIN_CH2, PIN_CH3, PIN_CH4};

WiFiClient gWifi;
PubSubClient gMqtt(gWifi);
bool gChannelOn[4] = {false, false, false, false};
uint32_t gLastStatusMs = 0;
uint32_t gLastWifiAttemptMs = 0;
bool gStatusDirty = true;

int levelFor(bool on) {
    return on ? CHANNEL_ACTIVE_LEVEL : CHANNEL_IDLE_LEVEL;
}

void driveOutputs() {
    if (!BOARD_PINS_CONFIRMED) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        if (kChannelPins[i] >= 0) {
            digitalWrite(kChannelPins[i], levelFor(gChannelOn[i]));
        }
    }
    if (PIN_STATUS_LED >= 0) {
        const bool anyOn = gChannelOn[0] || gChannelOn[1] || gChannelOn[2] || gChannelOn[3];
        digitalWrite(PIN_STATUS_LED, levelFor(anyOn));
    }
}

void publishStatus() {
    if (!gMqtt.connected()) {
        return;
    }
    char payload[512];
    snprintf(
        payload,
        sizeof(payload),
        "{\"device\":\"%s\",\"fw\":\"%s\",\"outputs_enabled\":%s,"
        "\"channels\":[{\"id\":1,\"state\":\"%s\"},{\"id\":2,\"state\":\"%s\"},"
        "{\"id\":3,\"state\":\"%s\"},{\"id\":4,\"state\":\"%s\"}],"
        "\"uptime_s\":%lu,\"rssi\":%d}",
        kClientId,
        kFirmwareVersion,
        BOARD_PINS_CONFIRMED ? "true" : "false",
        gChannelOn[0] ? "on" : "off",
        gChannelOn[1] ? "on" : "off",
        gChannelOn[2] ? "on" : "off",
        gChannelOn[3] ? "on" : "off",
        static_cast<unsigned long>(millis() / 1000UL),
        WiFi.RSSI());
    gMqtt.publish(kStatusTopic, payload, false);
    gStatusDirty = false;
    gLastStatusMs = millis();
}

void setChannel(int channel, bool on) {
    if (channel < 1 || channel > 4) {
        return;
    }
    gChannelOn[channel - 1] = on;
    driveOutputs();
    gStatusDirty = true;
    Serial.printf("channel %d -> %s (outputs %s)\n", channel, on ? "on" : "off",
                  BOARD_PINS_CONFIRMED ? "driven" : "held off");
}

void allOff() {
    for (int i = 0; i < 4; ++i) {
        gChannelOn[i] = false;
    }
    driveOutputs();
    gStatusDirty = true;
    Serial.println("all channels off");
}

// Accepts {"channel":1,"state":"on"} and {"channel":0,"state":"off"}.
void handleCommand(const char* payload) {
    const char* channelKey = strstr(payload, "\"channel\"");
    const char* stateKey = strstr(payload, "\"state\"");
    if (channelKey == nullptr || stateKey == nullptr) {
        Serial.println("command ignored: missing channel or state");
        return;
    }
    const char* channelColon = strchr(channelKey, ':');
    if (channelColon == nullptr) {
        Serial.println("command ignored: channel has no value");
        return;
    }
    const int channel = atoi(channelColon + 1);
    const bool on = strstr(stateKey, "\"on\"") != nullptr;
    const bool off = strstr(stateKey, "\"off\"") != nullptr;
    if (!on && !off) {
        Serial.println("command ignored: state must be on or off");
        return;
    }
    if (channel == 0 && off) {
        allOff();
        return;
    }
    if (channel >= 1 && channel <= 4) {
        setChannel(channel, on);
        return;
    }
    Serial.printf("command ignored: channel %d\n", channel);
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, kCommandTopic) != 0) {
        return;
    }
    char body[256];
    const unsigned int n = length < sizeof(body) - 1 ? length : sizeof(body) - 1;
    memcpy(body, payload, n);
    body[n] = '\0';
    handleCommand(body);
}

void ensureWifi() {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }
    const uint32_t now = millis();
    if (gLastWifiAttemptMs != 0 && now - gLastWifiAttemptMs < kWifiRetryMs) {
        return;
    }
    gLastWifiAttemptMs = now;
    Serial.printf("WiFi connecting to %s\n", WIFI_SSID);
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void ensureMqtt() {
    if (WiFi.status() != WL_CONNECTED || gMqtt.connected()) {
        return;
    }
    Serial.printf("MQTT connecting to %s as %s\n", MQTT_HOST, kClientId);
#if defined(MQTT_USER)
    const bool ok = gMqtt.connect(kClientId, MQTT_USER, MQTT_PASSWORD);
#else
    const bool ok = gMqtt.connect(kClientId);
#endif
    if (!ok) {
        Serial.printf("MQTT connect failed rc=%d\n", gMqtt.state());
        return;
    }
    gMqtt.subscribe(kCommandTopic, 1);
    gStatusDirty = true;
    Serial.println("MQTT connected");
}

void setupOutputsOff() {
    if (!BOARD_PINS_CONFIRMED) {
        Serial.println("GPIO outputs disabled until BOARD_PINS_CONFIRMED is 1");
        return;
    }
    for (int i = 0; i < 4; ++i) {
        if (kChannelPins[i] >= 0) {
            pinMode(kChannelPins[i], OUTPUT);
            digitalWrite(kChannelPins[i], CHANNEL_IDLE_LEVEL);
        }
    }
    if (PIN_STATUS_LED >= 0) {
        pinMode(PIN_STATUS_LED, OUTPUT);
        digitalWrite(PIN_STATUS_LED, CHANNEL_IDLE_LEVEL);
    }
}

void setupOta() {
    ArduinoOTA.setHostname(kClientId);
#if defined(OTA_PASSWORD)
    ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
    ArduinoOTA.onStart([]() { Serial.println("OTA start"); });
    ArduinoOTA.onEnd([]() { Serial.println("OTA end"); });
    ArduinoOTA.onError([](ota_error_t error) { Serial.printf("OTA error %u\n", error); });
    ArduinoOTA.begin();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n%s %s\n", kClientId, kFirmwareVersion);

    memset(gChannelOn, 0, sizeof(gChannelOn));
    setupOutputsOff();

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("esp32-4ch-thyristor");
    WiFi.setAutoReconnect(true);
    ensureWifi();

    gMqtt.setServer(MQTT_HOST, MQTT_PORT);
    gMqtt.setCallback(onMqttMessage);
    gMqtt.setBufferSize(512);

    setupOta();
}

void loop() {
    ensureWifi();
    if (WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();
        ensureMqtt();
        gMqtt.loop();
        if (gMqtt.connected() && (gStatusDirty || millis() - gLastStatusMs >= kStatusIntervalMs)) {
            publishStatus();
        }
    }
    delay(10);
}
