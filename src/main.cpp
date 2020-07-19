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
const char *propertyHost = "maiden.pagekite.me";

time_t timeNow;
time_t timeNTP;

int hourOfDay;
int dayOfMonth;
int hourOn = 8;
int durationOn = 1;
int override = 0;
int overrideHour = 0;

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 50
#define RELAY_BUS 0
//#define LED_1 1
#define ONE_WIRE_BUS 2

bool relayOn = false;

// Define NTP Client to get time
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 2 * 60 * 60;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

ESP8266WebServer http_rest_server(HTTP_REST_PORT);

void charToStringL(const char S[], String &D)
{
    byte at = 0;
    const char *p = S;
    D = "";

    while (*p++)
    {
        D.concat(S[at++]);
    }
}

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
    BlinkNTimes(LED_BUILTIN, 3, 500);
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

void set_defaults()
{
    durationOn = 1;
    hourOn = 21;
    override = 0;
}

boolean GetProperties()
{
    String url = "/MelektroApi/getbrandersettings";
    WiFiClient client;
    String response;
    if (client.connect(propertyHost, 80))
    {
        client.print(String("GET " + url) + " HTTP/1.1\r\n" +
                     "Host: " + propertyHost + "\r\n" +
                     "Connection: close\r\n" +
                     "\r\n");

        while (client.connected() || client.available())
        {
            if (client.available())
            {
                String line = client.readStringUntil('\n');
                if (line.indexOf("hourOn") > 0)
                {
                    response = line;
                    Serial.println("line="+line);
                }
            }
        }
        client.stop();
    }
    else
    {
        Serial.print("connection to ");
        Serial.print(propertyHost);
        Serial.println(url + " failed!");
        client.stop();
        set_defaults();
        return false;
    }
    
    StaticJsonBuffer<800> doc;
    JsonObject &root = doc.parseObject(response);

    if (!root.success())
    {
        Serial.println("deserializeJson failed");
        set_defaults();
    }

    const char *tempPtr;
    tempPtr = root["hourOn"];
    String tempStr;
    charToStringL(tempPtr, tempStr);
    hourOn = tempStr.toInt();
    Serial.print("hourOn=");
    Serial.println(hourOn);

    tempPtr = root["durationOn"];
    charToStringL(tempPtr, tempStr);
    durationOn = tempStr.toInt();
    Serial.print("durationOn=");
    Serial.println(durationOn);

    tempPtr = root["override"];
    charToStringL(tempPtr, tempStr);
    override = tempStr.toInt();
    Serial.print("override=");
    Serial.println(override);

    return true;
}

void doSwitch()
{
    timeNow = now();

    time_t timeCurrent = timeNTP + timeNow;
    hourOfDay = hour(timeCurrent);
    dayOfMonth = day(timeCurrent);

    if (override == 1 && !relayOn)
    {
        relayOn = true;
        digitalWrite(RELAY_BUS, LOW);
        overrideHour = hourOfDay+1;
        timeClient.update();
    }

    if ((override == 0 && digitalRead(RELAY_BUS) == LOW) || 
        (override == 1 && relayOn && hourOfDay > overrideHour))
    {
        relayOn = false;
        digitalWrite(RELAY_BUS, HIGH);
        override = 0;
        overrideHour = 0;
        timeClient.update();
    }

    if ((dayOfMonth % 2 == 0 || dayOfMonth == 31) && hourOfDay >= hourOn && hourOfDay < hourOn + durationOn && !relayOn)
    {
        relayOn = true;
        digitalWrite(RELAY_BUS, LOW);
        timeClient.update();
    }

    if (hourOfDay > hourOn + durationOn && digitalRead(RELAY_BUS) == LOW)
    {
        relayOn = false;
        digitalWrite(RELAY_BUS, HIGH);
    }
}

void get_status()
{
    GetProperties();
    doSwitch();

    BlinkNTimes(LED_BUILTIN, 2, 500);
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
        jsonObj["UtcTime"] = GetCurrentTime(timeNTP + now());
        jsonObj["Hostname"] = hostName;
        jsonObj["IpAddress"] = WiFi.localIP().toString();
        jsonObj["MacAddress"] = WiFi.macAddress();
        jsonObj["Gpio_Relay"] = RELAY_BUS;
        jsonObj["DeviceType"] = "Relay";
        jsonObj["Status"] = relayOn;
        jsonObj["GPIOPin Status"] = digitalRead(RELAY_BUS);
        jsonObj["hourOn"] = hourOn;
        jsonObj["durationOn"] = durationOn;
        jsonObj["override"] = override;
        jsonObj["dayOfMonth"] = dayOfMonth;
        jsonObj["hourOfDay"] = hourOfDay;
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
                              "Welcome to the ESP8266 REST Web Server: " + GetCurrentTime(timeNTP + now()));
    });
    http_rest_server.on(URI, HTTP_GET, get_status);
}

void setup(void)
{
    Serial.begin(115200);
    pinMode(RELAY_BUS, OUTPUT);
    digitalWrite(RELAY_BUS, HIGH);

#ifdef DEBUG
    deviceCount = 5;
#else
#endif

    set_defaults();
    relayOn = false;
    if (init_wifi() == WL_CONNECTED)
    {
        Serial.print("Connected to ");
        Serial.print(ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
        timeClient.begin();

        config_rest_server_routing();

        http_rest_server.begin();
        Serial.println("HTTP REST Server Started");
        timeClient.update();
        timeNTP = timeClient.getEpochTime();

        GetProperties();
   }
    else
    {
        Serial.print("Error connecting to: ");
        Serial.println(ssid);
    }
}

void loop(void)
{
    doSwitch();
    http_rest_server.handleClient();
}