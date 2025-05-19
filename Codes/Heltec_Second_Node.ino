#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <esp_now.h>
#include <WiFi.h>

// Dirección MAC del receptor ESP-NOW
uint8_t peer_addr[] = {0xA0, 0xA3, 0xB3, 0x19, 0x2E, 0xF8}; // <- CAMBIA ESTO según tu caso

SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Configuración LoRa
#define RF_FREQUENCY        915000000
#define TX_OUTPUT_POWER     14
#define LORA_BANDWIDTH      0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE     1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT  0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define RX_TIMEOUT_VALUE 1000
#define BUFFER_SIZE 128

char rxpacket[BUFFER_SIZE];
bool lora_idle = true;

static RadioEvents_t RadioEvents;
int16_t rssi, rxSize;

// CALLBACK de ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("ESP-NOW Envío de datos: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "✅ Éxito" : "❌ Falló");
}

// Inicializa ESP-NOW
void initESPNow() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error inicializando ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peer_addr, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ No se pudo agregar el peer");
  } else {
    Serial.println("✅ Peer agregado");
  }
}

void VextON(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextOFF(void) {
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext, HIGH);
}

void setup() {
  Serial.begin(115200);
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

  WiFi.mode(WIFI_STA);  // Requerido para ESP-NOW
  initESPNow();

  VextON();
  delay(100);
  factory_display.init();
  factory_display.clear();
  factory_display.display();

  // LoRa
  RadioEvents.RxDone = OnRxDone;
  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
}

void loop() {
    Radio.IrqProcess();
    if (lora_idle) {
        Serial.println("⏳ Esperando paquetes LoRa...");
        lora_idle = false;
        Radio.Rx(0); // RX continua
    }
}


// 📡 Recepción LoRa
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  rxSize = size;
  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Radio.Sleep();

  String receivedData = String(rxpacket);
  receivedData.trim();
  Serial.printf("\n📡 Paquete recibido: \"%s\" con RSSI: %d\n", rxpacket, rssi);

  // 🔍 Extraer valores del JSON {"mq135":123,"mq2":456,"dust":789}
  int start1 = receivedData.indexOf("\"mq135\":") + 8;
  int end1 = receivedData.indexOf(",", start1);
  if (end1 == -1) end1 = receivedData.indexOf("}", start1);
  String mq135Value = receivedData.substring(start1, end1);
  mq135Value.trim();

  int start2 = receivedData.indexOf("\"mq2\":") + 6;
  int end2 = receivedData.indexOf(",", start2);
  if (end2 == -1) end2 = receivedData.indexOf("}", start2);
  String mq2Value = receivedData.substring(start2, end2);
  mq2Value.trim();

  int start3 = receivedData.indexOf("\"dust\":") + 7;
  int end3 = receivedData.indexOf("}", start3);
  String dustValue = receivedData.substring(start3, end3);
  dustValue.trim();

  if (mq135Value.length() == 0 || mq2Value.length() == 0 || dustValue.length() == 0) {
    Serial.println("❌ Formato JSON inválido.");
    lora_idle = true;
    return;
  }

  // 📺 Mostrar en pantalla OLED
  factory_display.clear();
  factory_display.setFont(ArialMT_Plain_10);
  factory_display.setTextAlignment(TEXT_ALIGN_CENTER);
  factory_display.drawString(64, 0, "Valores del Sensor");
  factory_display.setTextAlignment(TEXT_ALIGN_LEFT);
  factory_display.drawString(0, 10, "MQ135: " + mq135Value);
  factory_display.drawString(0, 25, "MQ2:   " + mq2Value);
  factory_display.drawString(0, 40, "Polvo: " + dustValue);
  factory_display.display();

  // 📤 JSON para enviar por ESP-NOW (formato correcto)
  String jsonString = "{\"mq135\":" + mq135Value + ",\"mq2\":" + mq2Value + ",\"dust\":" + dustValue + "}";
  Serial.println("📤 Enviando por ESP-NOW: " + jsonString);

  // Envío por ESP-NOW
  esp_err_t result = esp_now_send(peer_addr, (uint8_t *)jsonString.c_str(), jsonString.length());
  if (result != ESP_OK) {
    Serial.println("❌ Error al enviar el paquete por ESP-NOW");
  }

  lora_idle = true;
}
