#include <stdio.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentials.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define ONE_WIRE_BUS 2

const long utcOffsetInSeconds = 0;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float tempSensor1;
DeviceAddress Thermometer;
uint8_t sensor1[8];

String deviceAddress = "";
byte gpio = 2;
String temperature = "-127";
String ipAddress = "127.0.0.0";
String hostname = "";

ESP8266WebServer http_rest_server(HTTP_REST_PORT);

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
    Serial.println();
    hostname = WiFi.hostname();
    Serial.println("hostname = "+hostname);
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
    unsigned long epochTime = timeClient.getEpochTime();
    char buff[32];
    sprintf(buff, "%02d-%02d-%02d %02d:%02d:%02d",
            year(epochTime),
            month(epochTime),
            day(epochTime),
            hour(epochTime),
            minute(epochTime),
            second(epochTime));
    String currentTime = buff;

    Serial.println(currentTime);

    sensors.requestTemperatures();
    tempSensor1 = sensors.getTempC(sensor1);

    deviceAddress = GetAddressToString(Thermometer);
    temperature = tempSensor1;
    ipAddress = WiFi.localIP().toString();

    StaticJsonBuffer<400> jsonBuffer;
    JsonObject &jsonObj = jsonBuffer.createObject();
    char JSONmessageBuffer[400];

    if (deviceAddress.equals(""))
    {
        Serial.print("No Content");
        http_rest_server.send(204);
    }
    else
    {
        jsonObj["UtcTime"] = currentTime;
        jsonObj["EpochTime"] = epochTime;
        jsonObj["ThermometerId"] = deviceAddress;
        jsonObj["Temperature"] = temperature;
        jsonObj["Hostname"] = hostname;
        jsonObj["IpAddress"] = ipAddress;
        jsonObj["Gpio"] = gpio;
        jsonObj.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
        http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
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
        Serial.print("Connected to ");
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