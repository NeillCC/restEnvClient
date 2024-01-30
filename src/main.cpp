#pragma region Include Statements
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <HTTPClient.h>           //For making HTTP requests/posts
#include <Wire.h>                 //Serial I2C library
#include <Adafruit_BME280.h>      //BME280 Library
#include <ArduinoJson.h>          //For creating HTTP content and WiFiManager
#include <WiFiManager.h>          //Save WiFi Connection and provide config webUI
#ifdef ESP32
  #include <SPIFFS.h>             //For saving WiFiManager files
#endif
using namespace std;
#pragma endregion
#pragma region Custom Classes
class haUpdate {
  public:
    void updateSensor(DynamicJsonDocument httpPayload, string httpURL, std::string authToken) {
      DynamicJsonDocument httpPostJSONDoc(256); //TODO make smaller?
      char charPayload[128];
      HTTPClient http;
      serializeJson(httpPayload,charPayload);
      http.begin(httpURL.c_str());
      http.addHeader("Authorization",authToken.c_str());
      http.addHeader("Content-Type","application/json");
      http.POST(charPayload);
      http.end();
    }
};
#pragma endregion
#pragma region WiFiManager globals
bool shouldSaveConfig = false; //For WiFiManager
void saveConfigCallback() {
    //callback notifying us of the need to save config from webUI
    shouldSaveConfig = true;
  }
char homeAssistantServer[40] = "homeassistant.local.tld";
char homeAssistantPort[6] = "8123";
char homeAssistantToken[200] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI2ZGE1ZDBkZTRhZGQ0ZDYwODRhM2I3NDNhNWFiNTYyMiIsImlhdCI6MTY5MTg5NTk2MCwiZXhwIjoyMDA3MjU1OTYwfQ.w44abWoDy-_fuA0oCJTDQXBWsGvKpzCR_p93KBu05bs";
char hostname[16] = "hall1";
char friendlyName[30] = "Hallway 1";
char sleepSeconds[4] = "5";
#pragma endregion
#pragma region Setup //All steps are run in setup to enable deep sleep
void setup() {
////SETUP
  HTTPClient http;              //Instantiate httpClient to send data to HomeAssistant server
  int dhtPIN = 15;              //Which pin is the DHT data on
  int buttonPIN = 4;            //Pin to use for reset button
  int i2cSDA = 21;              //SDA Pin. ESP32 - 21
  int i2cSCL = 22;              //SCL Pin. ESP32 - 22
  float seaLevelHPA = 1013.25;  //
  Serial.begin(9600);
  Serial.println();
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
#pragma region Sensor Checks
  TwoWire bmeI2C = TwoWire(0);                      //Create empty TwoWire
  Adafruit_BME280 bme;                              //Create Empty BME object
  bool bmeStatus;                                   //Create bool for TwoWire connection status
  bmeI2C.begin(i2cSDA, i2cSCL, 100000);             //Begin TwoWire connection for I2C
  bmeStatus = bme.begin(0x76, &bmeI2C);             //Connect to sensor on I2C address 0x76
  float bmeTemperature = bme.readTemperature();     //Check Temperature. Returns Celsius
  float humidity = bme.readHumidity();              //Read humidity from sensor
  float hPa = bme.readPressure();                   //Read pressure from sensor
#pragma endregion
#pragma region URL Building
//Build URL Strings
  std::string connectURL ("http://"); //Protocol - Does not support https:// currently
  connectURL.append(homeAssistantServer);
  connectURL.append(":");
  connectURL.append(homeAssistantPort);
//Build API Strings
  std::string apiStateURL (connectURL);
  apiStateURL.append("/api/states/sensor.");
  apiStateURL.append(hostname);
//HTTP auth string
  std::string authToken ("Bearer ");
  authToken.append(homeAssistantToken);
#pragma endregion
#pragma region JSON
  haUpdate celz;
//Create empty JSON doc
  DynamicJsonDocument httpPostJSONDoc(256);  //TODO make smaller?
//Generate JSON for bmeTemperature in celsius and move to char array
//Static properties
  httpPostJSONDoc["attributes"]["unique_id"] = hostname;
//Dynamic properties
  httpPostJSONDoc["state"] = round(bmeTemperature*100)/100.00; //Read Celsius var and round to 2 decimal places
  httpPostJSONDoc["attributes"]["unit_of_measurement"] = "°C";
  httpPostJSONDoc["attributes"]["friendly_name"] = (std::string(friendlyName) + " Celsius");
  std::string httpTempC (apiStateURL+"_Celsius");
  celz.updateSensor(httpPostJSONDoc, httpTempC, authToken);
//Generate JSON for bmeTemperature in fahrenheit
  httpPostJSONDoc["state"] = round((bmeTemperature*9/5+32)*100)/100.00; //Convert Celsius to Fahrenheit and round to 2 decimal places
  httpPostJSONDoc["attributes"]["unit_of_measurement"] = "°F";
  httpPostJSONDoc["attributes"]["friendly_name"] = (std::string(friendlyName) + " Fahrenheit");
  std::string httpTempF (apiStateURL+"_Fahrenheit");
  celz.updateSensor(httpPostJSONDoc, httpTempF, authToken);
//Generate JSON for humidity
  httpPostJSONDoc["state"] = round(humidity*100)/100.00;
  httpPostJSONDoc["attributes"]["unit_of_measurement"] = "%";
  httpPostJSONDoc["attributes"]["friendly_name"] = (std::string(friendlyName) + " Humidity");
  std::string httpHumidity (apiStateURL+"_Humidity");
  celz.updateSensor(httpPostJSONDoc, httpHumidity, authToken);
//Generate JSON for pressure
  httpPostJSONDoc["state"] = round(hPa)/100.00; //Pressure
  httpPostJSONDoc["attributes"]["unit_of_measurement"] = "hPa";
  httpPostJSONDoc["attributes"]["friendly_name"] = (std::string(friendlyName) + " Barometer");
  std::string httpPressure (apiStateURL+"_Pressure");
  celz.updateSensor(httpPostJSONDoc, httpPressure, authToken);
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
#pragma region LOOP
void loop () {

}
#pragma endregion