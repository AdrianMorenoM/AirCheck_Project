// Pines de los sensores
#define MQ135_PIN A0
#define MQ2_PIN   A1
#define GP2Y_LED_PIN 7
#define GP2Y_VO_PIN  A2

void setup() {
  pinMode(GP2Y_LED_PIN, OUTPUT); // LED del sensor de polvo
  Serial.begin(115200); // Comunicación hacia la Heltec
}

void loop() {
  // Leer MQ135 y MQ2
  int mq135_value = analogRead(MQ135_PIN);
  int mq2_value   = analogRead(MQ2_PIN);

  // Activar LED del sensor de polvo
  digitalWrite(GP2Y_LED_PIN, LOW);     // Enciende el LED
  delayMicroseconds(280);
  int dust_value = analogRead(GP2Y_VO_PIN);
  delayMicroseconds(40);
  digitalWrite(GP2Y_LED_PIN, HIGH);    // Apaga el LED

  // Enviar todos los datos como JSON por serial
  Serial.print("{\"mq135\":");
  Serial.print(mq135_value);
  Serial.print(",\"mq2\":");
  Serial.print(mq2_value);
  Serial.print(",\"dust\":");
  Serial.print(dust_value);
  Serial.println("}");

  delay(1000); // Espera 1 segundo entre envíos
}
