#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <AsyncMqttClient.h>
#include <Adafruit_Si7021.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include "config.hpp"
#include "mqtt_discovery.hpp"
#include "ve_direct_text.hpp"

#define MAX_LINE 200

AsyncMqttClient mqttClient;
const uint16_t discoveryPort = 2112;
const uint16_t localPort = 2113;
MQTTDiscovery mqttDiscovery(discoveryPort,
                            localPort,
                            &mqttClient);

Adafruit_Si7021 tempHumSensor;

void doTempHumSensor();

const int reportRate_ms = 1000;
unsigned long nextThingMillis;

struct VicInput
{
  char mqttBase[1024];
  HardwareSerial *port;
  VEDirectText processor;
  char readLine[MAX_LINE];
  int pos;
};

VicInput inputs[3];

Config config;

void setup()
{
  if (!SPIFFS.begin(true))
  {
    delay(1000);
    ESP.restart();
  }

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile)
  {
    delay(1000);
    ESP.restart();
  }

  if (!config.readConfig(configFile))
  {
    delay(1000);
    ESP.restart();
  }
  configFile.close();

  WiFi.mode(WIFI_STA);
  WiFi.begin(config.getSSID(), config.getKey());
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    delay(1000);
    ESP.restart();
  }

  ArduinoOTA.setHostname(config.getMDNS());
  ArduinoOTA.begin();

  tempHumSensor = Adafruit_Si7021();
  if (!tempHumSensor.begin())
  {
    delay(1000);
    ESP.restart();
  }

  mqttDiscovery.discoverAndConnectBroker();

  // Load victron defs
  File victronDDFile = SPIFFS.open("/victron_data_def.json", "r");
  if (!victronDDFile)
  {
    Serial.println("Failed to open victron_data_def.json for reading");
    return;
  }
  else
  {
    VEDirectText::loadDefs(victronDDFile);
  }
  victronDDFile.close();

  // Done with files
  SPIFFS.end();

  // Initialize inputs
  Serial.begin(19200);
  strcpy(inputs[0].mqttBase, "pmcg-esp32/victron/bmv-712");
  inputs[0].port = &Serial;
  inputs[0].pos = 0;

  Serial1.begin(19200, SERIAL_8N1, 12, 14);
  strcpy(inputs[1].mqttBase, "pmcg-esp32/victron/solar/100-50");
  inputs[1].port = &Serial1;
  inputs[1].pos = 0;

  Serial2.begin(19200);
  strcpy(inputs[2].mqttBase, "pmcg-esp32/victron/solar/100-30");
  inputs[2].port = &Serial2;
  inputs[2].pos = 0;

  nextThingMillis = millis() + reportRate_ms;
}

void loop()
{
  ArduinoOTA.handle();

  if (millis() > nextThingMillis)
  {
    doTempHumSensor();

    nextThingMillis += reportRate_ms;
  }

  for (int i = 0; i < 3; i++)
  {
    DynamicJsonDocument updates(4096);
    while (inputs[i].port->available())
    {
      inputs[i].readLine[inputs[i].pos] = inputs[i].port->read();
      if ((!(inputs[i].pos < MAX_LINE)) || (inputs[i].readLine[inputs[i].pos] == '\n'))
      {
        // Process line
        inputs[i].readLine[inputs[i].pos] = 0;

        inputs[i].processor.handleLine(updates, inputs[i].readLine);

        inputs[i].pos = 0;
      }
      else
      {
        inputs[i].pos++;
      }
    }

    if ((!updates.isNull()) && mqttClient.connected())
    {
      char json[1024];
      char topic[500];
      char key[50];

      JsonObject obj = updates.as<JsonObject>();
      for (JsonPair kv : obj)
      {
        serializeJsonPretty(kv.value(), json);
        strcpy(key, kv.key().c_str());
        int keyLen = strlen(key);
        for (int i = 0; i < keyLen; i++)
        {
          if (key[i] == '#')
          {
            key[i] = '-';
          }
        }
        sprintf(topic, "%s/%s", inputs[i].mqttBase, key);
        mqttClient.publish(topic, 0, false, json, strlen(json));
      }

      // Serial.print("Updates available, sending: ");
      // Serial.println(json);

      // serializeJsonPretty(currentData, json);
      // Serial.print("Current data: ");
      // Serial.println(json);
    }
  }
}

void doTempHumSensor()
{
  if (mqttClient.connected())
  {
    char buf[20];

    sprintf(buf, "%.02f", tempHumSensor.readHumidity());
    mqttClient.publish("pmcg-esp32/humidity", 0, false, buf, strlen(buf));

    sprintf(buf, "%.02f", tempHumSensor.readTemperature());
    mqttClient.publish("pmcg-esp32/temperature", 0, false, buf, strlen(buf));
  }
}