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
float tempSensor[5];
DeviceAddress Thermometer[5];
uint8_t sensor[5][8];

String deviceAddress[5] = {"", "", "", "", ""};
byte gpio = 2;
String strTemperature[5] = {"-127", "-127", "-127", "-127", "-127"};
String ipAddress = "127.0.0.0";
String hostname = "";
int deviceCount;

ESP8266WebServer http_rest_server(HTTP_REST_PORT);

void BlinkNTimes(int pin, int blinks, unsigned long millies)
{
    digitalWrite(pin, LOW);
    for (int i = 0; i < blinks; i++)
    {
        digitalWrite(pin, HIGH);
        delay(millies);
        digitalWrite(pin, LOW);
        delay(millies);
    }
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
    Serial.println();
    hostname = WiFi.hostname();
    Serial.println("hostname = " + hostname);
    return WiFi.status();
    BlinkNTimes(LED_0, 3, 500);
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

String GetCurrentTime()
{
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
    return currentTime;
}

void get_temps()
{
    StaticJsonBuffer<600> jsonBuffer;
    JsonObject &jsonObj = jsonBuffer.createObject();
    char JSONmessageBuffer[600];
    
    try
    {
        jsonObj["DeviceCount"] = deviceCount;
        sensors.requestTemperatures();
        jsonObj["Nommer="] = "2";
        /*for (int i = 0; i < deviceCount; i++)
        {
            jsonObj["Nommer="] = "2.1";
            tempSensor[i] = sensors.getTempC(sensor[i]);
            jsonObj["Nommer="] = "2.2";
            deviceAddress[i] = GetAddressToString(Thermometer[i]);
            jsonObj["Nommer="] = "2.3";
            strTemperature[i] = tempSensor[i];
            jsonObj["Nommer="] = "2.4";
        }

        ipAddress = WiFi.localIP().toString();

        if (deviceCount == 0)
        {
            Serial.print("No Content");
            http_rest_server.send(204);
        }
        else
        {
            jsonObj["UtcTime"] = GetCurrentTime();
            jsonObj["DeviceCount"] = deviceCount;
            jsonObj["Hostname"] = hostname;
            jsonObj["IpAddress"] = ipAddress;
            jsonObj["Gpio"] = gpio;
            jsonObj["Nommer="] = "3";

            for (int i = 0; i < deviceCount; i++)
            {
                jsonObj["ThermometerId" + i] = deviceAddress[i];
            }
        }
        */
    }
    catch (const std::exception &e)
    {
        // String exception = e.what();
        // jsonObj["Exception"] = exception.substring(0, 99);
        jsonObj["Exception"] = " ";
        //std::cerr << e.what() << '\n';
    }
    
    jsonObj.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
    
    http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
    
    http_rest_server.send(200, "application/json", JSONmessageBuffer);
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []() {
        http_rest_server.send(200, "text/html",
                              "Welcome to the ESP8266 REST Web Server: " + GetCurrentTime());
    });
    http_rest_server.on("/temps", HTTP_GET, get_temps);
}

void setup(void)
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(LED_0, OUTPUT);

    sensors.begin();

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

    deviceCount =sensors.getDeviceCount();
    for (int j = 0; j < deviceCount; j++)
    {
        if (sensors.getAddress(Thermometer[0], 0))
        {
            for (uint8_t i = 0; i < 8; i++)
            {
                sensor[j][i] = Thermometer[0][i];
            }
        }
    }
}

void loop(void)
{
    http_rest_server.handleClient();
}