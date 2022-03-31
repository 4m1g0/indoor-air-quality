#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include "MHZ19.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "SSD1306Wire.h"
#include "config/config.h"
#include "SparkFun_SCD30_Arduino_Library.h"
#include "SdsDustSensor.h"
#include <SoftwareSerial.h>

// pin for pwm reading
//#define CO2_IN D8 // orange 

// pin for uart reading
#define MH_Z19_RX D4  // D7 yellow
#define MH_Z19_TX D0  // D6 striped yellow 

#define MH_Z19B_RX D7  // D7 yellow
#define MH_Z19B_TX D6  // D6 striped yellow 

#define SDS011_RX D3  // 
#define SDS011_TX D5  // 

// Power 5V
MHZ19 co2;
MHZ19 co2B;
SoftwareSerial serialMHZ19(MH_Z19_RX, MH_Z19_TX);
SoftwareSerial serialMHZ19B(MH_Z19B_RX, MH_Z19B_TX);

SdsDustSensor sds(SDS011_RX, SDS011_TX);

WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);
void reconnect();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void calibrateAllCo2();

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
  display.drawString(128/2, 50, "Connecting...");
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

  //wifiClient.setCACert(DSTroot_CA);
  wifiClient.setFingerprint(fingerprint);
  client.setServer(MQTT_ADDR, MQTT_PORT); 
  client.setCallback(mqttCallback);
  delay(100);
  reconnect();

  display.clear();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(128/2, 20, "Co2 Monitor");
  display.setFont(ArialMT_Plain_10);
  display.drawString(128/2, 50, "Preparing...");
  display.display();

  // enable debug to get addition information
  // co2.setDebug(true);
  if (airSensor.begin() == false)
  {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }

  serialMHZ19.begin(9600);
  co2.begin(serialMHZ19);
  co2.autoCalibration();

  serialMHZ19B.begin(9600);
  co2B.begin(serialMHZ19B);
  co2B.autoCalibration();

  sds.begin();
  delay(20000); // 20 seconds so the sensors have time to boot
  Serial.println(sds.queryFirmwareVersion().toString()); // prints firmware version
  Serial.println(sds.setActiveReportingMode().toString()); // ensures sensor is in 'active' reporting mode
  Serial.println(sds.setCustomWorkingPeriod(1).toString()); // ensures sensor has continuous working period - default but not recommended
}

void loop() {
  Serial.print("\n----- Time from start: ");
  Serial.print(millis() / 1000);
  Serial.println(" s");

  int ppm_uart = co2.getCO2();
  int ppmb_uart = co2B.getCO2();
  Serial.print("PPMuart: ");

  Serial.print(" ");
  if (ppm_uart > 0) {
    Serial.print(ppm_uart);
  } else {
    Serial.print("n/a");
  }

  Serial.print(" PPMB: ");
  if (ppmb_uart > 0) {
    Serial.print(ppmb_uart);
  } else {
    Serial.print("n/a");
  }

  int ppm_pwm = 0;/*co2.readCO2PWM();
  Serial.print(", PPMpwm: ");
  Serial.print(ppm_pwm);*/

  int temperature = co2.getTemperature();
  int temperatureB = co2B.getTemperature();
  Serial.print(", Temperature: ");

  if (temperature > 0) {
    Serial.println(temperature);
  } else {
    Serial.println("n/a");
  }

  if (temperatureB > 0) {
    Serial.println(temperatureB);
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

  PmResult pm = sds.readPm();

  if (pm.isOk()) {
    Serial.print("PM2.5 = ");
    Serial.print(pm.pm25);
    Serial.print(", PM10 = ");
    Serial.println(pm.pm10);

    // if you want to just print the measured values, you can use toString() method as well
    Serial.println(pm.toString());
  } else {
    // notice that loop delay is set to 0.5s and some reads are not available
    Serial.print("Could not read values from sensor, reason: ");
    Serial.println(pm.statusToString());
  }


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
               ",\"tempB\":" + String(temperatureB) +
               ",\"co2B\":" + String(ppmb_uart) +
               ",\"pm25\":" + String(pm.pm25) +
               ",\"pm10\":" + String(pm.pm10) +
               ",\"tStart\":" + String(millis()/1000) +
               "}";
  client.publish("iot/co2/habitacion1", json.c_str());

  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(128/2, 0, "Air Monitor");
  display.drawLine(30,11,98, 11);

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  String temperatureStr = "Temp: " + String(temperature) + String(" / ") + String(tempSensirion) + String(" / ") + String(temperatureB) + String(" ºC");
  display.drawString(2, 12, temperatureStr);

  String co2Str = /*"Co2: " + */String(ppm_uart) + String(" / ") + String(co2Sensirion) + String(" / ") + String(ppmb_uart) + String(" PPM");
  display.drawString(2, 26, co2Str);

  String humidityStr = "Humidity: " + String(humidity) + String(" %");
  display.drawString(2, 40, humidityStr);

  String pmStr = "PM2.5: " + String(pm.pm25) + String(" PM10: ") + String(pm.pm10) + String("PPM");
  display.drawString(2, 54, pmStr);
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

  //wifiClient = WiFiClientSecure();               // Wifi Client reconnect issue 4497 (https://github.com/esp8266/Arduino/issues/4497) (From Tasmota)
  //client.setClient(wifiClient);

  if (client.connect(String(ESP.getChipId()).c_str(), MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("MQTT connected");
    client.subscribe("iot/co2/calibrate");
  } 
  else {  
    Serial.print("MQTT Connection failed. Code: ");
    Serial.println(client.state());
  }
}

void calibrateAllCo2()
{
  co2.calibrate();
  co2B.calibrate();
  airSensor.setForcedRecalibrationFactor(400);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char* resetTopic = "iot/co2/calibrate";
  if (!strcmp(resetTopic, topic)) {
    Serial.println("STARTING CALLIBRATION!");
    Serial.println("Make sure the sensor has been outside for at least 20 min before running callibration");
    calibrateAllCo2();
  }
}
