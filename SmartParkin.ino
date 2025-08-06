#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "HX711.h"
#include <Preferences.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
TwoWire customWire = TwoWire(0);
#define OLED_SDA 39
#define OLED_SCL 40
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &customWire, -1);

#define TRIG1 1  
#define ECHO1 2
#define TRIG2 4 
#define ECHO2 5

#define DT 41
#define SCK 42
HX711 scale;

const int kapasitas = 200;
int tersedia = kapasitas;

bool motorSedangLewat = false;
unsigned long lastEvent = 0;
const unsigned long delayAntarMotor = 3000;

const float batasJarak = 40.0; 
const float batasBerat = 30.0;

bool eventPending = false;
int pendingSensor = 0; 
unsigned long pendingTime = 0;
const unsigned long maxWeightDelay = 1500;

Preferences prefs;
unsigned long lastSave = 0;

void setup() {
  Serial.begin(115200);

  pinMode(TRIG1, OUTPUT); pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT); pinMode(ECHO2, INPUT);

  prefs.begin("parkir", false);
  tersedia = prefs.getInt("tersedia", kapasitas); 

  customWire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED gagal");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  scale.begin(DT, SCK);
  scale.set_scale(15188.679);
  scale.tare();

  tampilkanKapasitas();
}

float bacaJarak(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long durasi = pulseIn(echoPin, HIGH, 30000);
  float jarak = durasi == 0 ? -1 : durasi * 0.034 / 2;
  if (jarak > 0 && jarak < 1) jarak = 999;
  return jarak;
}

void tampilkanKapasitas() {
  display.clearDisplay();
  display.setCursor(0, 10);
  display.println("KAPASITAS TERSEDIA:");
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print(tersedia);
  display.print("/");
  display.print(kapasitas);
  display.display();
  display.setTextSize(1);
}

void loop() {
  float jarakLuar = bacaJarak(TRIG1, ECHO1);
  float jarakDalam = bacaJarak(TRIG2, ECHO2);
  float berat = scale.get_units(5);
  if (berat < 0) berat = 0;

  Serial.print("Jarak Luar: "); Serial.print(jarakLuar); Serial.print(" cm | ");
  Serial.print("Jarak Dalam: "); Serial.print(jarakDalam); Serial.print(" cm | ");
  Serial.print("Berat: "); Serial.print(berat); Serial.println(" kg");

  unsigned long sekarang = millis();

  if (!eventPending && !motorSedangLewat) {
    if (jarakLuar > 0 && jarakLuar < batasJarak) {
      eventPending = true;
      pendingSensor = 1;
      pendingTime = sekarang;
      Serial.println("🔹 Deteksi awal: LUAR");
    }
    else if (jarakDalam > 0 && jarakDalam < batasJarak) {
      eventPending = true;
      pendingSensor = 2; 
      pendingTime = sekarang;
      Serial.println("🔹 Deteksi awal: DALAM");
    }
  }

  if (eventPending && berat < batasBerat && (sekarang - pendingTime > maxWeightDelay)) {
    Serial.println("⛔ Event dibatalkan (tidak ada beban)");
    eventPending = false;
  }

  if (eventPending && berat >= batasBerat && (sekarang - pendingTime <= 3000)) {
    if (pendingSensor == 1 && jarakDalam > 0 && jarakDalam < batasJarak && (sekarang - lastEvent > delayAntarMotor)) {
      tersedia = max(0, tersedia - 1);
      tampilkanKapasitas();
      Serial.println("✅ MOTOR MASUK");
      motorSedangLewat = true;
      lastEvent = sekarang;
      eventPending = false;
    }
    else if (pendingSensor == 2 && jarakLuar > 0 && jarakLuar < batasJarak && (sekarang - lastEvent > delayAntarMotor)) {
      tersedia = min(kapasitas, tersedia + 1);
      tampilkanKapasitas();
      Serial.println("✅ MOTOR KELUAR");
      motorSedangLewat = true;
      lastEvent = sekarang;
      eventPending = false;
    }
  }

  if (motorSedangLewat && jarakLuar > batasJarak && jarakDalam > batasJarak) {
    motorSedangLewat = false;
    lastEvent = millis();
    Serial.println("🔄 Reset jalur kosong");
  }

  if (millis() - lastSave > 10000) {
    prefs.putInt("tersedia", tersedia);
    lastSave = millis();
  }

  delay(100);
}
