#include <stdio.h>
#include <ArduinoJson.h>
#include <Arduino.h>

#include <credentials.h>

#include <WiFiUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

//#include <ArduinoOTA.h>

//Defined in credentials.h
//const char *propertyHost = "xxx.yyy.com";
//const char* url = "/MelektroApi/getbrandersettings";

//#define DEBUG
IPAddress staticIP(192, 168, 63, 60);
#define URI "/status"
IPAddress gateway(192, 168, 63, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dnsGoogle(8, 8, 8, 8);
const char *hostName = "brander";

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

char *GetCurrentTime(time_t epochTime)
{
    char *buff = new char[32];

    sprintf(buff, "%02d-%02d-%02d %02d:%02d:%02d",
            year(epochTime),
            month(epochTime),
            day(epochTime),
            hour(epochTime),
            minute(epochTime),
            second(epochTime));
    return buff;
}

void set_defaults()
{
    durationOn = 1;
    hourOn = 21;
    override = 0;
}

boolean GetProperties()
{
    WiFiClient client;
    String response;
    if (client.connect(propertyHost, 80))
    {
        client.print(String("GET " + (String)url) + " HTTP/1.1\r\n" +
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
                    Serial.println("line=" + line);
                }
            }
        }
        client.stop();
    }
    else
    {
        Serial.print("connection to ");
        Serial.print(propertyHost);
        Serial.print(url);
        Serial.println(" failed!");
        client.stop();
        set_defaults();
        return false;
    }

    StaticJsonDocument<800> doc;
    auto error = deserializeJson(doc, response);

    if (error)
    {
        Serial.println("deserializeJson failed");
        set_defaults();
    }

    const char *tempPtr;
    tempPtr = doc["hourOn"];
    hourOn = ((String)tempPtr).toInt();
    Serial.print("hourOn=");
    Serial.println(hourOn);

    tempPtr = doc["durationOn"];
    durationOn = ((String)tempPtr).toInt();
    Serial.print("durationOn=");
    Serial.println(durationOn);

    tempPtr = doc["override"];
    override = ((String)tempPtr).toInt();
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
        overrideHour = hourOfDay + 1;
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
    StaticJsonDocument<800> doc;
    //JsonObject jsonObj = jsonBuffer.createObject();
    char JSONmessageBuffer[800];

    try
    {
#ifdef DEBUG
        jsonObj["DEBUG"] = "******* true *******";
#else
        doc["DEBUG"] = "false";
#endif
        doc["UtcTime"] = GetCurrentTime(timeNTP + now());
        doc["Hostname"] = hostName;
        doc["IpAddress"] = WiFi.localIP().toString();
        doc["MacAddress"] = WiFi.macAddress();
        doc["Gpio_Relay"] = RELAY_BUS;
        doc["DeviceType"] = "Relay";
        doc["Status"] = relayOn;
        doc["GPIOPin Status"] = digitalRead(RELAY_BUS);
        doc["hourOn"] = hourOn;
        doc["durationOn"] = durationOn;
        doc["override"] = override;
        doc["dayOfMonth"] = dayOfMonth;
        doc["hourOfDay"] = hourOfDay;
    }
    catch (const std::exception &e)
    {
        // String exception = e.what();
        // jsonObj["Exception"] = exception.substring(0, 99);
        doc["Exception"] = " ";
        //std::cerr << e.what() << '\n';
    }

    serializeJsonPretty(doc, JSONmessageBuffer);

    String s;
    serializeJsonPretty(doc, s);

    http_rest_server.sendHeader("Access-Control-Allow-Origin", "*");
    http_rest_server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");

    http_rest_server.send(200, "application/json", JSONmessageBuffer);
}

void config_rest_server_routing()
{
    http_rest_server.on("/", HTTP_GET, []() {
        http_rest_server.send(200, "text/html",
                              "Welcome to the ESP8266 REST Web Server: " + (String)GetCurrentTime(timeNTP + now()));
    });
    http_rest_server.on(URI, HTTP_GET, get_status);
}

// void AotStuff()
// {
//     ArduinoOTA.onStart([]() {
//         String type;
//         if (ArduinoOTA.getCommand() == U_FLASH)
//         {
//             type = "sketch";
//         }
//         else
//         { // U_FS
//             type = "filesystem";
//         }

//         // NOTE: if updating FS this would be the place to unmount FS using FS.end()
//         Serial.println("Start updating " + type);
//     });
//     ArduinoOTA.onEnd([]() {
//         Serial.println("\nEnd");
//     });
//     ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
//         Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
//     });
//     ArduinoOTA.onError([](ota_error_t error) {
//         Serial.printf("Error[%u]: ", error);
//         if (error == OTA_AUTH_ERROR)
//         {
//             Serial.println("Auth Failed");
//         }
//         else if (error == OTA_BEGIN_ERROR)
//         {
//             Serial.println("Begin Failed");
//         }
//         else if (error == OTA_CONNECT_ERROR)
//         {
//             Serial.println("Connect Failed");
//         }
//         else if (error == OTA_RECEIVE_ERROR)
//         {
//             Serial.println("Receive Failed");
//         }
//         else if (error == OTA_END_ERROR)
//         {
//             Serial.println("End Failed");
//         }
//     });
//     ArduinoOTA.begin();
// }

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

        //AotStuff();

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