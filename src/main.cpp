#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <iostream>
#include <string>
#include <ArduinoJson.h>
using namespace std;

//Setup
//All steps are run in setup to enable deep sleep
void setup() {
////VARS
//Setup - wifissid, wifipassword, hostname, friendlyname, homeassistant server, homeassistant port, homeassistant api key
//Device shortname. max 15 chars. Like "hall1"
  char hostname[] = "hall1";
//Device friendly name. Like "Hallway 1"
  std::string friendlyName = "Hallway 1";
//WiFi must be 2.4g
//WiFi Network Name
  const char wifiSSID[] = "iot7";
//WiFi Password
  const char wifiPASS[] = "aDayattherange69!";
//The address of your HomeAssistant server. Accepts DNS or IPv4
  const String homeAssistantFQDN = "docker2.int.h4rb.bid";
//Web port of HomeAssistant. Default is 8123
  int homeAssistantPORT = 8123;
//HTTP Authorization token
  const char homeAssistantAuthTOKEN[] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI2ZGE1ZDBkZTRhZGQ0ZDYwODRhM2I3NDNhNWFiNTYyMiIsImlhdCI6MTY5MTg5NTk2MCwiZXhwIjoyMDA3MjU1OTYwfQ.w44abWoDy-_fuA0oCJTDQXBWsGvKpzCR_p93KBu05bs";
//Which pin is the DHT data on
  int dhtPIN = 4;
//Cycle seconds between updates
  int sleepSeconds = 5;

////SETUP
  Serial.begin(9600);
  Serial.println();
  WiFi.begin(wifiSSID,wifiPASS);
  HTTPClient http;
  DHT dht;
  delay(1000);
  if (WiFi.status() != 3) {
    Serial.println("WiFi Error!!! Check config or wifi.");Serial.println(WiFi.status());
    }
  WiFi.setHostname("tempSensor");
  dht.setup(dhtPIN);
  Serial.println("Setup Done!");

////MAIN

//Check temperature and humidity
  float humidity = dht.getHumidity();
  float tempC = dht.getTemperature();
  float tempF = roundf(tempC*9/5+32);
//Build URL Strings
  std::string connectURL ("http://");
  connectURL.append(homeAssistantFQDN.c_str());
  connectURL.append(":");
  std::string homeAssistantPORTString = to_string(homeAssistantPORT);
  connectURL.append(homeAssistantPORTString);
//Build API Strings
  std::string apiStateURL (connectURL);
  apiStateURL.append("/api/states/sensor.");
  apiStateURL.append(hostname);
//HTTP method string
  std::string methodURL ("POST");
//HTTP auth string
  std::string authToken ("Bearer ");
  authToken.append(homeAssistantAuthTOKEN);
//Create empty JSON doc
  DynamicJsonDocument doc(1024);
//Generate JSON for temperature in celsius and move to char array
  //Static properties
  doc["attributes"]["unique_id"] = hostname;
//Dynamic properties
  doc["state"] = tempC;
  doc["attributes"]["unit_of_measurement"] = "°C";
  doc["attributes"]["friendly_name"] = (friendlyName+" Celsius").c_str();
  char celsiusPAYLOAD[256];
  serializeJson(doc,celsiusPAYLOAD);
//Generate JSON for temperature in fahrenheit and move to char array
  doc["state"] = tempF;
  doc["attributes"]["unit_of_measurement"] = "°F";
  doc["attributes"]["friendly_name"] = (friendlyName+" Fahrenheit").c_str();
  char fahrenheitPAYLOAD[256];
  serializeJson(doc,fahrenheitPAYLOAD);
//Generate JSON for humidity
  doc["state"] = humidity;
  doc["attributes"]["unit_of_measurement"] = "humidity";
  doc["attributes"]["friendly_name"] = (friendlyName+" Humidity").c_str();
  char humidityPAYLOAD[256];
  serializeJson(doc,humidityPAYLOAD);
//Build URLs
// std::string httpTempC (apiStateURL + ".celsius");
// std::string httpTempF (apiStateURL + ".fahrenheit");
// std::string httpHumidity (apiStateURL + ".humidity");
  std::string httpTempC (apiStateURL+"_Celsius");
  std::string httpTempF (apiStateURL+"_Fahrenheit");
  std::string httpHumidity (apiStateURL+"_Humidity");
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
//Deep sleep
  Serial.println("Sleeping...");
  esp_deep_sleep(sleepSeconds*1000000);
}

void loop () {

}