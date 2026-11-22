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

constexpr char kFirmwareVersion[] = "0.2.2";
constexpr char kClientId[] = "PlatformIO_esp32-4ch-thyristor-board-illumination";
constexpr char kDhcpHostname[] = "esp32-4ch-thyristor";
constexpr char kStatusTopic[] = "illumination/4ch-thyristor/status";
constexpr char kCommandTopic[] = "illumination/4ch-thyristor/command/set";
constexpr char kOtaStatusTopic[] = "illumination/4ch-thyristor/ota/status";
constexpr char kOtaProgressTopic[] = "illumination/4ch-thyristor/ota/progress";
constexpr uint16_t kOtaPort = 3232;
constexpr uint32_t kStatusIntervalMs = 30000;
constexpr uint32_t kWifiRetryMs = 5000;

constexpr int kChannelPins[4] = {PIN_CH1, PIN_CH2, PIN_CH3, PIN_CH4};

WiFiClient gWifi;
PubSubClient gMqtt(gWifi);
bool gChannelOn[4] = {false, false, false, false};
uint32_t gLastStatusMs = 0;
uint32_t gLastWifiAttemptMs = 0;
bool gStatusDirty = true;
bool gOtaActive = false;
bool gOtaReady = false;

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

void publishRaw(const char* topic, const char* payload) {
    if (!gMqtt.connected()) {
        return;
    }
    gMqtt.publish(topic, payload, false);
}

void publishStatus() {
    if (!gMqtt.connected()) {
        return;
    }
    const IPAddress ip = WiFi.localIP();
    char payload[640];
    snprintf(
        payload,
        sizeof(payload),
        "{\"device\":\"%s\",\"fw\":\"%s\",\"outputs_enabled\":%s,"
        "\"ip\":\"%u.%u.%u.%u\",\"ota_port\":%u,"
        "\"channels\":[{\"id\":1,\"state\":\"%s\"},{\"id\":2,\"state\":\"%s\"},"
        "{\"id\":3,\"state\":\"%s\"},{\"id\":4,\"state\":\"%s\"}],"
        "\"uptime_s\":%lu,\"rssi\":%d}",
        kClientId,
        kFirmwareVersion,
        BOARD_PINS_CONFIRMED ? "true" : "false",
        ip[0],
        ip[1],
        ip[2],
        ip[3],
        static_cast<unsigned>(kOtaPort),
        gChannelOn[0] ? "on" : "off",
        gChannelOn[1] ? "on" : "off",
        gChannelOn[2] ? "on" : "off",
        gChannelOn[3] ? "on" : "off",
        static_cast<unsigned long>(millis() / 1000UL),
        WiFi.RSSI());
    publishRaw(kStatusTopic, payload);
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
    if (gOtaActive) {
        Serial.println("command ignored: OTA in progress");
        return;
    }
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

void publishOtaStatus(const char* state, const char* detail) {
    char payload[256];
    snprintf(
        payload,
        sizeof(payload),
        "{\"device\":\"%s\",\"fw\":\"%s\",\"state\":\"%s\",\"detail\":\"%s\"}",
        kClientId,
        kFirmwareVersion,
        state,
        detail);
    publishRaw(kOtaStatusTopic, payload);
}

void publishOtaProgress(unsigned int progress, unsigned int total) {
    char payload[192];
    const unsigned int pct = total > 0 ? (progress * 100U) / total : 0U;
    snprintf(
        payload,
        sizeof(payload),
        "{\"device\":\"%s\",\"progress\":%u,\"total\":%u,\"percent\":%u}",
        kClientId,
        progress,
        total,
        pct);
    publishRaw(kOtaProgressTopic, payload);
}

void configureOta() {
    ArduinoOTA.setHostname(kDhcpHostname);
    ArduinoOTA.setPort(kOtaPort);
#if defined(OTA_PASSWORD)
    ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
    ArduinoOTA.onStart([]() {
        gOtaActive = true;
        allOff();
        Serial.println("OTA start — channels forced off");
        publishOtaStatus("started", "channels_off");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("OTA end");
        publishOtaStatus("completed", "rebooting");
        gOtaActive = false;
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int lastPct = 101;
        const unsigned int pct = total > 0 ? (progress * 100U) / total : 0U;
        if (pct != lastPct && (pct % 10U == 0U || pct == 100U)) {
            lastPct = pct;
            publishOtaProgress(progress, total);
            Serial.printf("OTA %u%%\n", pct);
        }
    });
    ArduinoOTA.onError([](ota_error_t error) {
        gOtaActive = false;
        char detail[32];
        snprintf(detail, sizeof(detail), "error_%u", static_cast<unsigned>(error));
        Serial.printf("OTA error %u\n", error);
        publishOtaStatus("error", detail);
    });
}

// ArduinoOTA.begin() before STA has an IP leaves port 3232 closed; bind once online.
void ensureOta() {
    if (WiFi.status() != WL_CONNECTED) {
        if (gOtaReady) {
            ArduinoOTA.end();
            gOtaReady = false;
        }
        return;
    }
    if (gOtaReady) {
        return;
    }
    ArduinoOTA.begin();
    gOtaReady = true;
    const IPAddress ip = WiFi.localIP();
    Serial.printf(
        "OTA listening on %u.%u.%u.%u:%u\n",
        ip[0],
        ip[1],
        ip[2],
        ip[3],
        kOtaPort);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n%s %s\n", kClientId, kFirmwareVersion);

    memset(gChannelOn, 0, sizeof(gChannelOn));
    setupOutputsOff();

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(kDhcpHostname);
    WiFi.setAutoReconnect(true);
    ensureWifi();

    gMqtt.setServer(MQTT_HOST, MQTT_PORT);
    gMqtt.setCallback(onMqttMessage);
    gMqtt.setBufferSize(768);

    configureOta();
}

void loop() {
    ensureWifi();
    if (WiFi.status() == WL_CONNECTED) {
        ensureOta();
        ArduinoOTA.handle();
        ensureMqtt();
        gMqtt.loop();
        if (!gOtaActive && gMqtt.connected() &&
            (gStatusDirty || millis() - gLastStatusMs >= kStatusIntervalMs)) {
            publishStatus();
        }
    } else {
        ensureOta();
    }
    delay(10);
}
