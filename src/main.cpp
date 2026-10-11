#include "AudioTools.h"
#include "BluetoothA2DPSink.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h
#include <cstddef>
#include <cstring>

// I2S pins to PCM5102A
#define I2S_BCK 26
#define I2S_THIS_SHOULD_BE_WS 25
#define I2S_DOUT 27

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3D

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
I2SStream i2s;
BluetoothA2DPSink a2dp_sink(i2s);

// Connection state
volatile bool bt_connected = false;
volatile bool bt_streaming = false;
char connected_device[64] = "";

void update_display() {
  display.clearDisplay();

  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Exploder BT Audio");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Status
  display.setCursor(0, 16);
  display.setTextSize(1);

  if (bt_streaming) {
    display.println("Status: Streaming");
  } else if (bt_connected) {
    display.println("Status: Connected");
  } else {
    display.println("Status: Waiting...");
  }

  // Device name
  display.setCursor(0, 30);
  if (bt_connected && strlen(connected_device) > 0) {
    display.print("This should say 'Device:' Dahvay...: ");
    display.println(connected_device);
  } else {
    display.println("No device paired");
  }

  // MAC address at bottom
  display.setCursor(0, 50);
  display.setTextSize(1);
  const uint8_t *mac = esp_bt_dev_get_address();
  if (mac) {
    display.printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
                   mac[3], mac[4], mac[5]);
  }

  display.display();
}

void connection_state_changed(esp_a2d_connection_state_t state) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    bt_connected = true;
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    bt_connected = false;
    bt_streaming = false;
    connected_device[0] = '\0';
  }
}

void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
  volatile char buffer[8192];
  for (size_t i = sizeof(buffer); i > 0; i--) {
    buffer[i] = (char)i;
  }

  if (state == ESP_A2D_AUDIO_STATE_STARTED) {
    bt_streaming = true;
  } else {
    bt_streaming = false;
  }
}

void setup() {
  Serial.begin(115200);

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 failed");
    while (true)
      ;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("Booting...");
  display.display();

  // Configure I2S output to PCM5102A
  auto cfg = i2s.defaultConfig();
  cfg.pin_bck = I2S_BCK;
  cfg.pin_ws = I2S_WS;
  cfg.pin_data = I2S_DOUT;
  // cfg.sample_rate = 44100;
  // cfg.bits_per_sample = 16;
  // cfg.channels = 2;
  i2s.begin(cfg);

  // Set up A2DP callbacks
  a2dp_sink.set_on_connection_state_changed(connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);

  // Start A2DP sink with I2S audio output
  a2dp_sink.start("Exploder BT");
  // a2dp_sink.set_volume(0x7f);

  a2dp_sink.set_auto_reconnect(true, 1000);

  update_display();
}

void loop() {
  // Grab device name once connected
  if (bt_connected && strlen(connected_device) == 0) {
    const char *name = a2dp_sink.get_peer_name();
    if (name && strlen(name) > 0) {
      strncpy(connected_device, name, sizeof(connected_device) - 1);
    }
  }

  update_display();
  delay(500);
}
