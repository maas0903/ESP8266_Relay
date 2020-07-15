#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentials.h>

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

//#define DEBUG
IPAddress staticIP(192, 168, 63, 59);
#define URI "/status"
IPAddress gateway(192, 168, 63, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dnsGoogle(8, 8, 8, 8);
String hostName = "brander";

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define RELAY_BUS 0
#define LED_1 1
#define ONE_WIRE_BUS 2

bool relayOn = false;

// Define NTP Client to get time
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 0;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

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

    WiFi.config(staticIP, gateway, subnet, dnsGoogle);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(hostName);
    WiFi.begin(ssid, password);

    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY))
    {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    Serial.println();
    BlinkNTimes(LED_1, 3, 500);
    return WiFi.status();
}

String GetCurrentTime(time_t epochTime)
{
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

void get_status()
{
    BlinkNTimes(LED_1, 2, 500);
    StaticJsonBuffer<800> jsonBuffer;
    JsonObject &jsonObj = jsonBuffer.createObject();
    char JSONmessageBuffer[800];

    try
    {
#ifdef DEBUG
        jsonObj["DEBUG"] = "******* true *******";
#else
        jsonObj["DEBUG"] = "false";
#endif
        jsonObj["UtcTime"] = GetCurrentTime(now());
        jsonObj["Hostname"] = hostName;
        jsonObj["IpAddress"] = WiFi.localIP().toString();
        jsonObj["MacAddress"] = WiFi.macAddress();
        jsonObj["Gpio_Relay"] = RELAY_BUS;
        jsonObj["DeviceType"] = "Relay";
        //jsonObj["DeviceCount"] = deviceCount;

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
    http_rest_server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");

    http_rest_server.send(200, "application/json", JSONmessageBuffer);
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []() {
        http_rest_server.send(200, "text/html",
                              "Welcome to the ESP8266 REST Web Server: " + GetCurrentTime(now()));
    });
    http_rest_server.on(URI, HTTP_GET, get_status);
}

void setup(void)
{
    Serial.begin(115200);
    pinMode(RELAY_BUS, OUTPUT);

#ifdef DEBUG
    deviceCount = 5;
#else
#endif

    if (init_wifi() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
        timeClient.begin();
        timeClient.update();

        config_rest_server_routing();

        http_rest_server.begin();
        Serial.println("HTTP REST Server Started");
    }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }
}

void loop(void)
{
    time_t time_now = now();

    Serial.println("Current time: " + GetCurrentTime(time_now));

    int hourOfDay = hour(time_now);
    int dayOfMonth = day(time_now);

    if ((dayOfMonth % 2 == 0 || dayOfMonth == 31) && hourOfDay >= 12 && !relayOn)
    {
        relayOn = true;
        digitalWrite(RELAY_BUS, HIGH);
        timeClient.update();
    }

    if (hourOfDay > 13 && digitalRead(RELAY_BUS) == HIGH)
    {
        relayOn = false;
        digitalWrite(RELAY_BUS, LOW);
    }

    http_rest_server.handleClient();
}