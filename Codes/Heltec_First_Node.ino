#include "LoRaWan_APP.h"
#include "Arduino.h"
#include <Wire.h>

// Configuración LoRa
#define RF_FREQUENCY        915000000 // Hz (ajustar según región)
#define TX_OUTPUT_POWER     5
#define LORA_BANDWIDTH      0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE     1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT  0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false

#define RX_TIMEOUT_VALUE 1000
#define BUFFER_SIZE 128

char txpacket[BUFFER_SIZE];
bool lora_idle = true;

static RadioEvents_t RadioEvents;

void OnTxDone(void);
void OnTxTimeout(void);

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, 43, 44); // RX=43, TX=44

    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;

    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
    Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                      LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                      LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                      true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
}

void loop() {
    if (Serial2.available()) {
        String receivedData = Serial2.readStringUntil('\n');
        receivedData.trim();

        Serial.println("📥 JSON recibido: " + receivedData);

        if (receivedData.startsWith("{") && receivedData.endsWith("}")) {
            int mq135Index = receivedData.indexOf("\"mq135\":");
            int mq2Index = receivedData.indexOf("\"mq2\":");
            int dustIndex = receivedData.indexOf("\"dust\":");

            if (mq135Index != -1 && mq2Index != -1 && dustIndex != -1) {
                // Extraer MQ135
                int comma1 = receivedData.indexOf(",", mq135Index);
                String mq135Value = receivedData.substring(mq135Index + 8, comma1);
                mq135Value.trim();

                // Extraer MQ2
                int comma2 = receivedData.indexOf(",", mq2Index);
                String mq2Value = receivedData.substring(mq2Index + 6, comma2);
                mq2Value.trim();

                // Extraer DUST
                int endBrace = receivedData.indexOf("}", dustIndex);
                String dustValue = receivedData.substring(dustIndex + 7, endBrace);
                dustValue.trim();

                // Formar paquete con los tres sensores
                snprintf(txpacket, BUFFER_SIZE, "{\"mq135\":%s,\"mq2\":%s,\"dust\":%s}",
                         mq135Value.c_str(), mq2Value.c_str(), dustValue.c_str());

                Serial.printf("📡 Enviando por LoRa: %s\n", txpacket);

                if (lora_idle) {
                    Radio.Send((uint8_t *)txpacket, strlen(txpacket));
                    lora_idle = false;
                }
            } else {
                Serial.println("⚠️ Error: JSON válido pero falta alguna clave esperada.");
            }
        } else {
            Serial.println("❌ Error: Formato JSON inválido.");
        }
    }

    Radio.IrqProcess();
}

void OnTxDone(void) {
    Serial.println("✅ Transmisión LoRa completada.");
    lora_idle = true;
}

void OnTxTimeout(void) {
    Radio.Sleep();
    Serial.println("❌ Error: Timeout en transmisión LoRa.");
    lora_idle = true;
}
