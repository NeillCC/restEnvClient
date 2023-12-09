#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <Arduino.h>              //Arduino Library
#include <HTTPClient.h>           //For making HTTP requests/posts
#include <DHT.h>                  //Temperature Sensor Library
#include <string>                 //For manipulating strings
#include <ArduinoJson.h>          //For creating HTTP content and WiFiManager
#include <WiFiManager.h>          //Save WiFi Connection and provide config webUI
#ifdef ESP32
  #include <SPIFFS.h>             //For saving WiFiManager files
#endif
using namespace std;
#pragma region WiFiManager globals
bool shouldSaveConfig = false; //For WiFiManager
void saveConfigCallback() {
    //callback notifying us of the need to save config
    shouldSaveConfig = true;
  }
char homeAssistantServer[40] = "homeassistant.local.tld";
char homeAssistantPort[6] = "8123";
char homeAssistantToken[200] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI2ZGE1ZDBkZTRhZGQ0ZDYwODRhM2I3NDNhNWFiNTYyMiIsImlhdCI6MTY5MTg5NTk2MCwiZXhwIjoyMDA3MjU1OTYwfQ.w44abWoDy-_fuA0oCJTDQXBWsGvKpzCR_p93KBu05bs";
char hostname[16] = "hall1";
char friendlyName[30] = "Hallway 1";
char sleepSeconds[4] = "5";
#pragma endregion
//Setup
//All steps are run in setup to enable deep sleep
void setup() {
#pragma region Setup
////SETUP
  HTTPClient http; //Instantiate httpClient to send data to HomeAssistant server
  int dhtPIN = 4; //Which pin is the DHT data on
  DHT dht; //Instantiate Temperature Sensor. Supports DHT11/22 without any changes
  Serial.begin(9600);
  Serial.println();
  dht.setup(dhtPIN); //Configure pin for use with Temperature Sensor
#pragma endregion
#pragma region File System Read
  if (SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        strcpy(homeAssistantServer, json["homeAssistantServer"]);
        strcpy(homeAssistantPort, json["homeAssistantPort"]);
        strcpy(homeAssistantToken, json["homeAssistantToken"]);
        strcpy(hostname, json["hostname"]);
        strcpy(friendlyName, json["friendlyName"]);
        strcpy(sleepSeconds, json["sleepSeconds"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      } else {
        Serial.println("Config file does not exist");
      }
    }
  //end read
#pragma endregion
#pragma region WiFiManager
  WiFiManager wifiManager; //Instantiate WiFiManager for wifi autoconnect and config portal
  WiFiManagerParameter homeAssistantServerWM("homeAssistantServerWM","Home Assistant Server DNS/IP", homeAssistantServer, 40);
  WiFiManagerParameter homeAssistantPortWM("homeAssistantPortWM","Home Assistant Port",homeAssistantPort, 6);
  WiFiManagerParameter homeAssistantTokenWM("homeAssistantTokenWM","Home Assistant Long-Lived access token",homeAssistantToken,200);
  WiFiManagerParameter hostnameWM("hostnameWM","Hostname",hostname,15);
  WiFiManagerParameter friendlyNameWM("friendlyNameWM","Friendly Name", friendlyName, 30);
  WiFiManagerParameter sleepSecondsWM("sleepSeconds","Seconds between updates [5-300]",sleepSeconds,4); //Amount of seconds to sleep before updating
  wifiManager.addParameter(&homeAssistantServerWM); //The address of your HomeAssistant server. Accepts DNS or IPv4
  wifiManager.addParameter(&homeAssistantPortWM); //The port of your HomeAssistant server. Default is 8123
  wifiManager.addParameter(&homeAssistantTokenWM); //Long-lived Access Token from HomeAssistant
  wifiManager.addParameter(&hostnameWM); //Device shortname. max 15 chars. Like "hall1"
  wifiManager.addParameter(&friendlyNameWM); //Device friendly name. Like "Hallway 1"
  wifiManager.addParameter(&sleepSecondsWM); //Amount of seconds to sleep between server updates
  wifiManager.setConfigPortalTimeout(180); //Seconds before reset if Wifi is not available
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConnectRetries(3);
  wifiManager.setConnectTimeout(10);
  wifiManager.setHostname(hostnameWM.getValue());
  wifiManager.autoConnect("ESP32_TempProbe","password"); //Launch self-hosted wifi to configure device
  //Copy webUI strings to variables
  strcpy(homeAssistantServer, homeAssistantServerWM.getValue());
  strcpy(homeAssistantPort, homeAssistantPortWM.getValue());
  strcpy(homeAssistantToken, homeAssistantTokenWM.getValue());
  strcpy(hostname, hostnameWM.getValue());
  strcpy(friendlyName, friendlyNameWM.getValue());
  strcpy(sleepSeconds, sleepSecondsWM.getValue());
  //change hostname
  //delay(1000); //Wait 1 second for WiFi to connect
  Serial.println("Setup Done!");
#pragma endregion
#pragma region Temperature Checking
//Check temperature and humidity
  float humidity = dht.getHumidity();
  float tempC = dht.getTemperature();
  float tempF = roundf(tempC*9/5+32); //Convert Celsius to Fahrenehit using MATH!
#pragma endregion
#pragma region URL Building
//Build URL Strings
  std::string connectURL ("http://"); //Protocol - Does not support https:// currently
  connectURL.append(homeAssistantServer);
  connectURL.append(":");
  //std::string homeAssistantPORTString = to_string(homeAssistantPORT);
  connectURL.append(homeAssistantPort);
//Build API Strings
  std::string apiStateURL (connectURL);
  apiStateURL.append("/api/states/sensor.");
  apiStateURL.append(hostname);
//Build URLs
  std::string httpTempC (apiStateURL+"_Celsius");
  std::string httpTempF (apiStateURL+"_Fahrenheit");
  std::string httpHumidity (apiStateURL+"_Humidity");
//HTTP method string
  std::string methodURL ("POST");
//HTTP auth string
  std::string authToken ("Bearer ");
  authToken.append(homeAssistantToken);
#pragma endregion
#pragma region JSON
//Create empty JSON doc
  DynamicJsonDocument httpPostJSONDoc(1024);
//Generate JSON for temperature in celsius and move to char array
//Static properties
  httpPostJSONDoc["attributes"]["unique_id"] = hostname;
//Dynamic properties
  httpPostJSONDoc["state"] = tempC;
  httpPostJSONDoc["attributes"]["unit_of_measurement"] = "°C";
  httpPostJSONDoc["attributes"]["friendly_name"] = (string(friendlyName) + " Celsius");
  char celsiusPAYLOAD[256];
  serializeJson(httpPostJSONDoc,celsiusPAYLOAD);
//Generate JSON for temperature in fahrenheit and move to char array
  httpPostJSONDoc["state"] = tempF;
  httpPostJSONDoc["attributes"]["unit_of_measurement"] = "°F";
  httpPostJSONDoc["attributes"]["friendly_name"] = (string(friendlyName) + " Fahrenheit").c_str();
  char fahrenheitPAYLOAD[256];
  serializeJson(httpPostJSONDoc,fahrenheitPAYLOAD);
//Generate JSON for humidity
  httpPostJSONDoc["state"] = humidity;
  httpPostJSONDoc["attributes"]["unit_of_measurement"] = "humidity";
  httpPostJSONDoc["attributes"]["friendly_name"] = (string(friendlyName) + " Humidity").c_str();
  char humidityPAYLOAD[256];
  serializeJson(httpPostJSONDoc,humidityPAYLOAD);
#pragma endregion
#pragma region HTTP Posts
//Celsius
  Serial.println("Attempting Celsius Connection...");
  http.begin(httpTempC.c_str());
  http.addHeader("Authorization",authToken.c_str());
  http.addHeader("Content-Type","application/json");
  http.POST(celsiusPAYLOAD);
//Fahrenheit
  Serial.println("Attempting Farhrenheit Connection...");
  http.begin(httpTempF.c_str());
  http.addHeader("Authorization",authToken.c_str());
  http.addHeader("Content-Type","application/json");
  http.POST(fahrenheitPAYLOAD);
//Humidity
  Serial.println("Attempting Humidity Connection...");
  http.begin(httpHumidity.c_str());
  http.addHeader("Authorization",authToken.c_str());
  http.addHeader("Content-Type","application/json");
  http.POST(humidityPAYLOAD); 
#pragma endregion
#pragma region File System Save
  if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonDocument json(1024);
      json["homeAssistantServer"] = homeAssistantServer;
      json["homeAssistantPort"] = homeAssistantPort;
      json["homeAssistantToken"] = homeAssistantToken;
      json["hostname"] = hostname;
      json["friendlyName"] = friendlyName;
      json["sleepSeconds"] = sleepSeconds;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

      serializeJson(json, Serial);
      serializeJson(json, configFile);
      configFile.close();
      //end save
      Serial.println("local ip");
      Serial.println(WiFi.localIP());
  }
#pragma endregion
#pragma region Sleep
  Serial.println("Sleeping...");
  esp_deep_sleep(atoi(sleepSeconds)*1000000); //Deep sleep
#pragma endregion
} //End Setup

void loop () {

}