#include <LITTLEFS.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson - needs to be v5 not v6
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager   
#include <HTTPClient.h>
#include "driver/adc.h"
#include <esp_bt.h>
#define FORMAT_LITTLEFS_IF_FAILED true

// TODO:
// Giving up on https - sticking with http on the local network..
// break this out into separate functions?
// Sort out deep sleep
// https://www.youtube.com/watch?v=n_A_8Y4xNx8
// Test button push and toggle
// The variables don't seem to be sticking
// Need to pull a pin high to stop it waking up (probably)
// Get the circuit sorted and then put the bits together.

// New stuff to send things to the right server
char api_host[64] = "http://192.168.75.4";
char api_uri[64] = "api/getSwitchToggle?secret=SECRET_GOES_HERE";
char switch_id[2] = "2";
char this_device_id[3] = "10";
//flag for saving data
bool shouldSaveConfig = false;

// sort out the ADC business
const int VOLTAGEPIN = 36;
int VOLTAGEVALUE = 0;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

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
  // LITTLEFS.format();
    if(!LITTLEFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LITTLEFS Mount Failed");
        return;
    }
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LITTLEFS.begin()) {
    Serial.println("mounted file system");
    if (LITTLEFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LITTLEFS.open("/config.json", "r");
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

          strcpy(api_host, json["api_host"]);
          strcpy(api_uri, json["api_uri"]);
          strcpy(switch_id, json["switch_id"]);
          strcpy(this_device_id, json["this_device_id"]);
 
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

void goToDeepSleep()
{
  Serial.println("Going to the deep sleep...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  adc_power_off();
  esp_wifi_stop();
  esp_bt_controller_disable();

  // Go to sleep! Zzzz
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1); //1 = High, 0 = Low
  setupSpiffs();

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // Uncomment and run it once, if you want to erase all the stored information
  // wifiManager.resetSettings();
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  WiFiManagerParameter custom_api_host("api_host", "API host", api_host, 64);
  wifiManager.addParameter(&custom_api_host);
  WiFiManagerParameter custom_api_uri("api_uri", "API uri", api_uri, 64);
  wifiManager.addParameter(&custom_api_uri);
  WiFiManagerParameter custom_switch_id("switch_id", "switch_id(1-9)", switch_id, 1);
  wifiManager.addParameter(&custom_switch_id);
  WiFiManagerParameter custom_device_id("this_device_id", "this_device_id(1-99)", this_device_id, 2);
  wifiManager.addParameter(&custom_device_id);
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
 //timeSync.begin();
  //read updated parameters
  strcpy(api_host, custom_api_host.getValue());
  strcpy(api_uri, custom_api_uri.getValue());
  strcpy(switch_id, custom_switch_id.getValue());
  strcpy(this_device_id, custom_device_id.getValue());

//save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["api_host"] = api_host;
    json["api_uri"] = api_uri;
    json["switch_id"]   = switch_id;
    json["this_device_id"]   = this_device_id;

    // json["ip"]          = WiFi.localIP().toString();
    // json["gateway"]     = WiFi.gatewayIP().toString();
    // json["subnet"]      = WiFi.subnetMask().toString();

    File configFile = LITTLEFS.open("/config.json", "w");
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
  Serial.println("api_host: ");
  Serial.println(api_host);
  Serial.println("api_uri: ");
  Serial.println(api_uri);
  Serial.println("switch_id: ");
  Serial.println(switch_id);
   Serial.println("this_device_id: ");
  Serial.println(this_device_id);
// get the value from the pin
VOLTAGEVALUE = analogRead(VOLTAGEPIN);

// Only do it once before deep sleep
  String serverPath = String(api_host) + "/" + String(api_uri) + "&switch=" + String(switch_id) + "&this_device_id=" + String(this_device_id) + "&voltage=" + String(VOLTAGEVALUE);
//  Serial.println(serverPath);
  Serial.println(serverPath.c_str());

// Actually try to do something for a bit
  HTTPClient http;
  http.begin(serverPath.c_str());
      
      // Send HTTP GET request
      int httpResponseCode = http.GET();
      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
  delay (500);
//  fetch.GET(serverPath.c_str());  
//  while (fetch.busy())
//{
//    if (fetch.available())
//    {
//        Serial.println(fetch.read());
//    }
//}  
//  fetch.clean();
Serial.println("Time for deep sleep");
goToDeepSleep();
}

void loop(){

}