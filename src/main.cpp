#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <MHZ.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// pin for pwm reading
#define CO2_IN D2

// pin for uart reading
#define MH_Z19_RX D4  // D7
#define MH_Z19_TX D0  // D6

MHZ co2(MH_Z19_RX, MH_Z19_TX, CO2_IN, MHZ14A);

WiFiClient wifiClient;
PubSubClient client(wifiClient);
void reconnect();

void setup() {
  Serial.begin(9600);
  pinMode(CO2_IN, INPUT);
  delay(100);
  Serial.println("MHZ 19B");

  // setup wifi connection
  WiFi.mode(WIFI_STA);
  WiFi.begin("****", "****");
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

  client.setServer("***", 1883);
  delay(100);
  reconnect();

  // enable debug to get addition information
  // co2.setDebug(true);

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

  Serial.print(ppm_uart);
  Serial.print(" ");
  if (ppm_uart > 0) {
    Serial.print(ppm_uart);
  } else {
    Serial.print("n/a");
  }

  int ppm_pwm = co2.readCO2PWM();
  Serial.print(", PPMpwm: ");
  Serial.print(ppm_pwm);

  int temperature = co2.getLastTemperature();
  Serial.print(", Temperature: ");

  if (temperature > 0) {
    Serial.println(temperature);
  } else {
    Serial.println("n/a");
  }

  if (!client.connected()) { // Reconnect to MQTT if necesary
    reconnect();
    delay(20);
  }


  String json = "{\"ppmUART\":" + String(ppm_uart) + ",\"ppmPWM\":" + String(ppm_pwm) + ",\"temp\":" + String(temperature) + "}";
  client.publish("co2/habitacion1", json.c_str());

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

  if (client.connect(String(ESP.getChipId()).c_str(), "***", "****")) {
    Serial.println("MQTT connected");
  } 
  else {  
    Serial.println("MQTT Connection failed");
  }
}
