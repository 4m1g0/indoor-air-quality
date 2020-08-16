#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <MHZ.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "SSD1306Wire.h"
#include "config/config.h"
#include "SparkFun_SCD30_Arduino_Library.h"

// pin for pwm reading
//#define CO2_IN D8 // orange 

// pin for uart reading
#define MH_Z19_RX D4  // D7 yellow
#define MH_Z19_TX D0  // D6 striped yellow 

// Power 5V
MHZ co2(MH_Z19_RX, MH_Z19_TX, MHZ14A);

WiFiClient wifiClient;
PubSubClient client(wifiClient);
void reconnect();

SSD1306Wire  display(0x3c, SDA, SCL);
SCD30 airSensor;

void setup() {
  display.init();
  Serial.begin(9600);
  //pinMode(CO2_IN, INPUT);
  delay(100);
  Serial.println("MHZ 19B");

  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(128/2, 20, "Co2 Monitor");
  display.setFont(ArialMT_Plain_10);
  display.drawString(128/2, 50, "Preheating...");
  display.display();


  // setup wifi connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (!WiFi.getAutoConnect()) { WiFi.setAutoConnect(true); } 

  while (WiFi.status() != WL_CONNECTED)
  {
    static int i = 0;
    i++;
    if (i>120) // 1 min
      ESP.restart();
    Serial.print(".");
    delay(500);
  }

  client.setServer(MQTT_ADDR, 1883);
  delay(100);
  reconnect();

  // enable debug to get addition information
  // co2.setDebug(true);
  if (airSensor.begin() == false)
  {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }

  if (co2.isPreHeating()) {
    Serial.print("Preheating...");
    while (co2.isPreHeating()) {
      delay(100);
      client.loop();
    }
    Serial.println();
  }
}

void loop() {
  Serial.print("\n----- Time from start: ");
  Serial.print(millis() / 1000);
  Serial.println(" s");

  int ppm_uart = co2.readCO2UART();
  Serial.print("PPMuart: ");

  Serial.print(" ");
  if (ppm_uart > 0) {
    Serial.print(ppm_uart);
  } else {
    Serial.print("n/a");
  }

  int ppm_pwm = 0;/*co2.readCO2PWM();
  Serial.print(", PPMpwm: ");
  Serial.print(ppm_pwm);*/

  int temperature = co2.getLastTemperature();
  Serial.print(", Temperature: ");

  if (temperature > 0) {
    Serial.println(temperature);
  } else {
    Serial.println("n/a");
  }


  int co2Sensirion = airSensor.getCO2();
  Serial.print("co2(ppm):");
  Serial.print(co2Sensirion);

  int tempSensirion = airSensor.getTemperature();
  Serial.print(" temp(C):");
  Serial.print(tempSensirion, 1);

  int humidity = airSensor.getHumidity();
  Serial.print(" humidity(%):");
  Serial.print(humidity, 1);

  Serial.println();


  if (!client.connected()) { // Reconnect to MQTT if necesary
    reconnect();
    delay(20);
  }


  String json = "{\"ppmUART\":" + String(ppm_uart) +
               /*",\"ppmPWM\":" + String(ppm_pwm) + */
               ",\"temp\":" + String(temperature) +
               ",\"ppmSensirion\":" + String(co2Sensirion) +
               ",\"tempSensirion\":" + String(tempSensirion) +
               ",\"humidity\":" + String(humidity) +
               "}";
  client.publish("co2/habitacion1", json.c_str());

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(128/2, 2, "Co2 Monitor");

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  String temperatureStr = "Temp: " + String(temperature) + String(" ºC") + String(" / ") + String(tempSensirion) + String(" ºC");
  display.drawString(2, 22, temperatureStr);

  String co2Str = "Co2: " + String(ppm_uart) + String(" PPM") + String(" / ") + String(co2Sensirion) + String(" PPM");
  display.drawString(2, 36, co2Str);

  String humidityStr = "Humidity: " + String(humidity) + String(" %");
  display.drawString(2, 50, humidityStr);
  display.display();



  Serial.println("\n------------------------------");
  for (int i = 0; i < 100; i++)
  {
    delay(600);
    client.loop(); 
  }
}

void reconnect() {
  static unsigned long lastConnection = 0;

  if (client.connected() || millis() - lastConnection < 3000)
    return;
  
  lastConnection = millis();

  wifiClient = WiFiClient();               // Wifi Client reconnect issue 4497 (https://github.com/esp8266/Arduino/issues/4497) (From Tasmota)
  client.setClient(wifiClient);

  if (client.connect(String(ESP.getChipId()).c_str(), MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("MQTT connected");
  } 
  else {  
    Serial.println("MQTT Connection failed");
  }
}
