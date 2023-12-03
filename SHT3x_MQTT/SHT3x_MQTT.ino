#include <Arduino.h>

#include <WiFi.h>
#include <PubSubClient.h>
#include "Adafruit_SHT31.h"
#include <esp_task_wdt.h>
#define WDT_TIMEOUT 60

#include "auth.h"

const int freq = 5000;
const int ledChannel1 = 0;
const int ledChannel2 = 1;
const int resolution = 8;

const int LED_BRIGHTNESS = 16;

WiFiClient espClient;
PubSubClient client(espClient);

Adafruit_SHT31 sht31 = Adafruit_SHT31();

String wifiMacAddress;

#define LED_1  12
#define LED_2  13

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int times = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    times++;
    if (times == 100) {
      Serial.println();
      Serial.println("WIFI connect failed, RESTART!!!");
      ESP.restart();
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  wifiMacAddress = WiFi.macAddress();
  Serial.println(wifiMacAddress);
}


void mqtt_callback(char* topic, byte* message, unsigned int len) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < len; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  esp_task_wdt_reset();

  // Feel free to add more if statements to control more GPIOs with MQTT
  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  //  auto commandTopic = "OpenTherm/" + wifiMacAddress + "/Command";
  //  if (String(topic) == "esp32/output") {
  //    handleOutputTopic(messageTemp);
  //  } else if (String(topic) == commandTopic) {
  //    handleCommandTopic(messageTemp);
  //  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("connected");
      // Subscribe
      // auto commandTopic = "OpenTherm/" + wifiMacAddress + "/Command";
      String commandTopic = "OpenTherm/Sensor/SHT30";
      client.subscribe(commandTopic.c_str());
      Serial.println("Subscribe: " + commandTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  ledcSetup(ledChannel1, freq, resolution);
  ledcSetup(ledChannel2, freq, resolution);

  // TWO LEDS
  ledcAttachPin(LED_1, ledChannel1);
  ledcAttachPin(LED_2, ledChannel2);
  ledcWrite(ledChannel2, LED_BRIGHTNESS);

  // I2C GND
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  Wire.begin(10, 3);   // sda= GPIO_10 /scl= GPIO_3

  Serial.println("SHT31 test");
  if (!sht31.begin(0x44)) {
    // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  sht31.heater(false);

  Serial.print("Heater Enabled State: ");
  if (sht31.isHeaterEnabled()) {
    Serial.println("ENABLED");
  } else {
    Serial.println("DISABLED");
  }


  Serial.println("+ Starting client...");
  delay(100);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL); //add current thread to WDT watch
}

long lastPublishTime = millis();

void publishMQTT(float temperature, float humidity) {
  char str[80];
  snprintf(str, sizeof str, "%.2f℃ %.2f%%", temperature, humidity);
  String topic = "OpenTherm/Sensor/SHT30";
  // auto topic = "OpenTherm/" + wifiMacAddress + "/Status/SHT30";
  client.publish(topic.c_str(), str);
  Serial.println("MQTT published!");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  esp_task_wdt_reset();
  client.loop();
  esp_task_wdt_reset();

  ledcWrite(ledChannel1, LED_BRIGHTNESS);

  // __asm__ __volatile__ ("nop");
  // delayMicroseconds(1);
  // delay(1);

  long current = millis();
  if (current - lastPublishTime < 0 || current - lastPublishTime >= 10000) {
    lastPublishTime = current;

    // No LED means working on MQTT
    ledcWrite(ledChannel1, 0);
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    if (!isnan(t)) {  // check if 'is not a number'
      Serial.print("Temp *C = ");
      Serial.print(t);
      Serial.print("\t");
    } else {
      Serial.println("Failed to read temperature");
    }

    if (!isnan(h)) {  // check if 'is not a number'
      Serial.print("Hum. % = ");
      Serial.println(h);
    } else {
      Serial.println("Failed to read humidity");
    }
    esp_task_wdt_reset();
    publishMQTT(t, h);
    esp_task_wdt_reset();

    ledcWrite(ledChannel1, LED_BRIGHTNESS);
  }
}
