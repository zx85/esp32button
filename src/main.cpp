#include <LittleFS.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson - needs to be v5 not v6
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager   
#include <WiFiClient.h>


// TODO:
// break this out into separate functions?
// Sort out deep sleep
// https://www.youtube.com/watch?v=n_A_8Y4xNx8
// Test button push and toggle
// One more go at https ..?
// https://github.com/maakbaas/esp8266-iot-framework

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state

// New stuff to send things to the right server
char api_url[80] = "www.mus-ic.co.uk/indicator.txt";
char switch_id[2] = "1";
//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

// Assign output variables to GPIO pins
//const int red_light = 0;
//const int yellow_light = 2;
//String red_state = "0";
//String yellow_state= "0";

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void setupSpiffs(){
  //clean FS, for testing
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LittleFS.begin()) {
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(api_url, json["api_url"]);
          strcpy(switch_id, json["switch_id"]);
 
          // if(json["ip"]) {
          //   Serial.println("setting custom ip from config");
          //   strcpy(static_ip, json["ip"]);
          //   strcpy(static_gw, json["gateway"]);
          //   strcpy(static_sn, json["subnet"]);
          //   Serial.println(static_ip);
          // } else {
          //   Serial.println("no custom ip in config");
          // }

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void setup() {
  Serial.begin(115200);

  setupSpiffs();

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // Uncomment and run it once, if you want to erase all the stored information
  //wifiManager.resetSettings();
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  WiFiManagerParameter custom_api_url("api_url", "API url", api_url, 80);
  wifiManager.addParameter(&custom_api_url);
  WiFiManagerParameter custom_switch_id("switch_id", "switch_id(1-9)", switch_id, 1);
  wifiManager.addParameter(&custom_switch_id);
  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // if we still have not connected restart and try all over again
    ESP.restart();
    delay(5000);
  }

 Serial.println("wifi connected FTW");

  //read updated parameters
  strcpy(api_url, custom_api_url.getValue());
  strcpy(switch_id, custom_switch_id.getValue());

//save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["api_url"] = api_url;
    json["switch_id"]   = switch_id;

    // json["ip"]          = WiFi.localIP().toString();
    // json["gateway"]     = WiFi.gatewayIP().toString();
    // json["subnet"]      = WiFi.subnetMask().toString();

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  } 

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
  Serial.println("api_url: ");
  Serial.println(api_url);
  Serial.println("switch_id: ");
  Serial.println(switch_id);
  
}

void loop(){
  String serverPath = "http://"+String(api_url);
//  Serial.println(serverPath);
  WiFiClient client;
  HTTPClient http;
  http.begin(client,serverPath.c_str());
  int httpResponseCode = http.GET();
  if (httpResponseCode>0) {
   Serial.print("HTTP Response code: ");
   Serial.println(String(httpResponseCode)+"\n");
// we are in the good place
      String payload = http.getString();
      Serial.print("Payload: ");
      Serial.println(payload+"\n");
// 
  }
  else {
   Serial.print("Error code: ");
   Serial.println(String(httpResponseCode)+"\n");
   }
      // Free resources
   http.end();
  Serial.print("Ten seconds until next run \n");
   delay(10000);   
  }