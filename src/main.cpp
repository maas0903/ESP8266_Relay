#include <stdio.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentialsAP.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <WiFiUdp.h>
#include <NTPClient.h>

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define ONE_WIRE_BUS 2

const long utcOffsetInSeconds = 3600;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor1;
DeviceAddress Thermometer;
uint8_t sensor1[8];

struct Sensor18B20
{
    String deviceAddress;
    byte gpio;
    String temperature;
    String ipAddress;
} thermometer_resource;

ESP8266WebServer http_rest_server(HTTP_REST_PORT);

void init_thermometer_resource()
{
    thermometer_resource.deviceAddress = "";
    thermometer_resource.gpio = 2;
    thermometer_resource.temperature = "-127";
    thermometer_resource.ipAddress = "127.0.0.0";
}

int init_wifi()
{
    int retries = 0;

    Serial.println("Connecting to WiFi");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
    {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    return WiFi.status();
}

String GetAddressToString(DeviceAddress deviceAddress)
{
    String str = "";
    for (uint8_t i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16)
            str += String(0, HEX);
        str += String(deviceAddress[i], HEX);
    }
    return str;
}

void get_temps()
{
    Serial.print("HTTP Method: ");
    Serial.println(http_rest_server.method());

    timeClient.update();
    String currentTime= timeClient.getFormattedTime();
    Serial.println(currentTime);

    sensors.requestTemperatures();
    tempSensor1 = sensors.getTempC(sensor1);

    thermometer_resource.deviceAddress = GetAddressToString(Thermometer);
    thermometer_resource.temperature = tempSensor1;
    thermometer_resource.ipAddress = WiFi.localIP().toString();

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &jsonObj = jsonBuffer.createObject();
    char JSONmessageBuffer[200];

    if (thermometer_resource.deviceAddress.equals(""))
    {
        Serial.print("No Content");
        http_rest_server.send(204);
    }
    else
    {
        jsonObj["id"] = thermometer_resource.deviceAddress;
        jsonObj["gpio"] = thermometer_resource.gpio;
        jsonObj["UTC time"] = currentTime;
        jsonObj["temperature"] = thermometer_resource.temperature;
        jsonObj["ipaddress"] = thermometer_resource.ipAddress;
        jsonObj.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
        http_rest_server.send(200, "application/json", JSONmessageBuffer);
    }
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []() {
        http_rest_server.send(200, "text/html",
                              "Welcome to the ESP8266 REST Web Server");
    });
    http_rest_server.on("/temps", HTTP_GET, get_temps);
}

void setup(void)
{
    Serial.begin(115200);

    init_thermometer_resource();
    sensors.begin();
    if (sensors.getAddress(Thermometer, 0))
    {
        //Take the first sensor as the measuring sensor
        for (uint8_t i = 0; i < 8; i++)
        {
            sensor1[i] = Thermometer[i];
        }
    }

    if (init_wifi() == WL_CONNECTED)
    {
        Serial.print("Connetted to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }

    timeClient.begin();

    config_rest_server_routing();

    http_rest_server.begin();
    Serial.println("HTTP REST Server Started");
}

void loop(void)
{
    http_rest_server.handleClient();
}