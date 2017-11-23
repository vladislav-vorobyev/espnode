#include <ArduinoJson.h>
#include <FS.h>


class NodeConfig {
private:
  StaticJsonBuffer<1024> jsonBuffer;
public:
  bool useTermoSensor1; // user 1st temperature sensor
  bool useTermoSensor2; // user 2nd temperature sensor
  bool useAlarmSensor1; // user 1st alarm sensor
  bool useAlarmSensor2; // user 2nd alarm sensor
  float t1ShiftDelta; // value to correct the t1
  float t2ShiftDelta; // value to correct the t2
  int8_t alarmInit1; // 1st alarm sensor init state value (set -1 to auto detect)
  int8_t alarmInit2; // 2nd alarm sensor init state value (set -1 to auto detect)
  bool alarmActive; // is alarm check activated
  bool alarmTestMode; // is alarm test mode
  int alarmReadyDelay; // time to wait from start before initialize alarm pin normal state and be ready for alarm (sec)
  int alarmSkipDelay; // delay between WiFi activity and alarm sensors check (sec)
  int dataStoreDelay; // delay between send data to hosting (sec)
  int dataStoreAttempts; // attempts to resending if failed
  int dataStoreAttemptsDelay; // delay between attempts to resend (sec)
  String storeLogin; // store data page access
  String storePassword;
  String storeURL; // hosting
  bool iswifiConnectionCheck; // is make a ping the local wifi server to recheck connection
  String wifiRouterIP; // url to check wifi
  String raspberryURL; // local store url

  NodeConfig(){
    // set default values
    useTermoSensor1 = true;
    useTermoSensor2 = true;
    useAlarmSensor1 = true;
    useAlarmSensor2 = true;
    t1ShiftDelta = t2ShiftDelta = 0;
    alarmInit1 = alarmInit2 = -1;
    alarmActive = false;
    alarmTestMode = false;
    alarmReadyDelay = ALARM_INIT_WAIT;
    alarmSkipDelay = 0;
    dataStoreDelay = DATA_STORE_DEFAULT_DELAY;
    dataStoreAttempts = ATTEMPTS_TO_STORE_DATA;
    dataStoreAttemptsDelay = ATTEMPTS_DEFAULT_DELAY;
    storeLogin = "esp8266";
    storePassword = "123";
    storeURL = "http://vvvnet.ru/home/data.php";
    iswifiConnectionCheck = false;
    wifiRouterIP = "";
    raspberryURL = "http://192.168.0.4/data.php";
  }
  
  void read(const char* name, JsonObject& json, bool& var);
  void read(const char* name, JsonObject& json, int& var);
  void read(const char* name, JsonObject& json, int8_t& var);
  void read(const char* name, JsonObject& json, float& var);
  void read(const char* name, JsonObject& json, String& var);
  bool save();
  bool load();
  String getHTMLFormFields();
  void handleFormSubmit(ESP8266WebServer& server);
};

/*
* Read value
*/
void NodeConfig::read(const char* name, JsonObject& json, bool& var) {
  JsonVariant val = json[name];
  if (val.success()) var = val;
  Serial.println(String(name) + ": " + var);
}

void NodeConfig::read(const char* name, JsonObject& json, int& var) {
  JsonVariant val = json[name];
  if (val.success()) var = val;
  Serial.println(String(name) + ": " + var);
}

void NodeConfig::read(const char* name, JsonObject& json, int8_t& var) {
  JsonVariant val = json[name];
  if (val.success()) var = val;
  Serial.println(String(name) + ": " + var);
}

void NodeConfig::read(const char* name, JsonObject& json, float& var) {
  JsonVariant val = json[name];
  if (val.success()) var = val;
  Serial.println(String(name) + ": " + var);
}

void NodeConfig::read(const char* name, JsonObject& json, String& var) {
  JsonVariant val = json[name];
  if (val.success()) var = val.as<String>();
  Serial.println(String(name) + ": " + var);
}


/*
* Load Config
*/
bool NodeConfig::load() {
  Serial.println("Load config...");
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate a buffer to store contents of the file.
  std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  configFile.readBytes(buf.get(), size);

  //StaticJsonBuffer<1024> jsonBuffer;
  jsonBuffer.clear();
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }

  // read variables
  read("useTermoSensor1", json, useTermoSensor1);
  read("useTermoSensor2", json, useTermoSensor2);
  read("useAlarmSensor1", json, useAlarmSensor1);
  read("useAlarmSensor2", json, useAlarmSensor2);
  read("t1ShiftDelta", json, t1ShiftDelta);
  read("t2ShiftDelta", json, t2ShiftDelta);
  read("alarmInit1", json, alarmInit1);
  read("alarmInit2", json, alarmInit2);
  read("alarmActive", json, alarmActive);
  read("alarmTestMode", json, alarmTestMode);
  read("alarmReadyDelay", json, alarmReadyDelay);
  read("alarmSkipDelay", json, alarmSkipDelay);
  read("dataStoreDelay", json, dataStoreDelay);
  read("dataStoreAttempts", json, dataStoreAttempts);
  read("dataStoreAttemptsDelay", json, dataStoreAttemptsDelay);
  read("storeLogin", json, storeLogin);
  read("storePassword", json, storePassword);
  read("storeURL", json, storeURL);
  read("iswifiConnectionCheck", json, iswifiConnectionCheck);
  read("wifiRouterIP", json, wifiRouterIP);

  return true;
}

/*
* Save Config
*/
bool NodeConfig::save() {
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["useTermoSensor1"] = useTermoSensor1;
  json["useTermoSensor2"] = useTermoSensor2;
  json["useAlarmSensor1"] = useAlarmSensor1;
  json["useAlarmSensor2"] = useAlarmSensor2;
  json["t1ShiftDelta"] = t1ShiftDelta;
  json["t2ShiftDelta"] = t2ShiftDelta;
  json["alarmInit1"] = alarmInit1;
  json["alarmInit2"] = alarmInit2;
  json["alarmActive"] = alarmActive;
  json["alarmTestMode"] = alarmTestMode;
  json["alarmReadyDelay"] = alarmReadyDelay;
  json["alarmSkipDelay"] = alarmSkipDelay;
  json["dataStoreDelay"] = dataStoreDelay;
  json["dataStoreAttempts"] = dataStoreAttempts;
  json["dataStoreAttemptsDelay"] = dataStoreAttemptsDelay;
  json["storeLogin"] = storeLogin;
  json["storePassword"] = storePassword;
  json["storeURL"] = storeURL;
  json["iswifiConnectionCheck"] = iswifiConnectionCheck;
  json["wifiRouterIP"] = wifiRouterIP;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

/*
* Prepare HTML form fields
*/
String NodeConfig::getHTMLFormFields() {
  String response = "";
  response += "<input type='checkbox' name='useTermoSensor1' value=1 " + String(useTermoSensor1? "checked" : "") + "> use 1<sup>st</sup> temperature sensor with\n";
  response += "<input type='text' name='t1ShiftDelta' value='" + String(t1ShiftDelta) + "' style='width:60px'> shift<br>\n";
  response += "<input type='checkbox' name='useTermoSensor2' value=1 " + String(useTermoSensor2? "checked" : "") + "> use 2<sup>nd</sup> temperature sensor with\n";
  response += "<input type='text' name='t2ShiftDelta' value='" + String(t2ShiftDelta) + "' style='width:60px'> shift<br>\n";
  response += "<input type='checkbox' name='useAlarmSensor1' value=1 " + String(useAlarmSensor1? "checked" : "") + "> use 1<sup>st</sup> alarm sensor with\n";
  response += "normal state = <input type='text' name='alarmInit1' value='" + ((alarmInit1 != -1)? String(alarmInit1) : "") + "' placeholder='auto detect' style='width:30px'><br>\n";
  response += "<input type='checkbox' name='useAlarmSensor2' value=1 " + String(useAlarmSensor2? "checked" : "") + "> use 2<sup>nd</sup> alarm sensor with\n";
  response += "normal state = <input type='text' name='alarmInit2' value='" + ((alarmInit2 != -1)? String(alarmInit2) : "") + "' placeholder='auto detect' style='width:30px'><br>\n";
  response += "<input type='checkbox' name='alarmActive' value=1 " + String(alarmActive? "checked" : "") + "> alarm check activated<br>\n";
  response += "<input type='checkbox' name='alarmTestMode' value=1 " + String(alarmTestMode? "checked" : "") + "> alarm test mode <br>\n";
  response += "<input type='text' name='alarmReadyDelay' value='" + String(alarmReadyDelay) + "' style='width:60px'> wait from start before alarm sensor will be ready (sec)<br>\n";
  response += "<input type='text' name='alarmSkipDelay' value='" + String(alarmSkipDelay) + "' style='width:60px'> delay between WiFi activity and alarm check (sec)<br>\n";
  response += "<input type='text' name='dataStoreDelay' value='" + String(dataStoreDelay) + "' style='width:60px'> store delay (sec)<br>\n";
  response += "<input type='text' name='dataStoreAttempts' value='" + String(dataStoreAttempts) + "' style='width:60px'> store attempts<br>\n";
  response += "<input type='text' name='dataStoreAttemptsDelay' value='" + String(dataStoreAttemptsDelay) + "' style='width:60px'> store attempts delay (sec)<br>\n";
  response += "<input type='text' name='storeLogin' value='" + String(storeLogin) + "' style='width:200px'> store login<br>\n";
  response += "<input type='text' name='storePassword' value='" + String(storePassword) + "' style='width:200px'> store password<br>\n";
  response += "<input type='text' name='storeURL' value='" + String(storeURL) + "' style='width:200px'> store URL<br>\n";
  response += "<input type='checkbox' name='iswifiConnectionCheck' value=1 " + String(iswifiConnectionCheck? "checked" : "") + "> check connection to WiFi router<br>\n";
  response += "<input type='text' name='wifiRouterIP' value='" + String(wifiRouterIP) + "' style='width:200px'> WiFi router IP<br>\n";
  return response;
}

/*
* Handle the HTML form submit
*/
void NodeConfig::handleFormSubmit(ESP8266WebServer& server) {
  String val;
  useTermoSensor1 = server.arg("useTermoSensor1")!="";
  useTermoSensor2 = server.arg("useTermoSensor2")!="";
  useAlarmSensor1 = server.arg("useAlarmSensor1")!="";
  useAlarmSensor2 = server.arg("useAlarmSensor2")!="";
  t1ShiftDelta = ((val = server.arg("t1ShiftDelta")) != "")? atof(val.c_str()) : 0;
  t2ShiftDelta = ((val = server.arg("t2ShiftDelta")) != "")? atof(val.c_str()) : 0;
  alarmInit1 = ((val = server.arg("alarmInit1")) != "")? atoi(val.c_str()) : -1;
  alarmInit2 = ((val = server.arg("alarmInit2")) != "")? atoi(val.c_str()) : -1;
  alarmActive = server.arg("alarmActive")!="";
  alarmTestMode = server.arg("alarmTestMode")!="";
  if ((val = server.arg("alarmReadyDelay")) != "") alarmReadyDelay = atoi(val.c_str());
  if ((val = server.arg("alarmSkipDelay")) != "") alarmSkipDelay = atoi(val.c_str());
  if ((val = server.arg("dataStoreDelay")) != "") dataStoreDelay = atoi(val.c_str());
  if ((val = server.arg("dataStoreAttempts")) != "") dataStoreAttempts = atoi(val.c_str());
  if ((val = server.arg("dataStoreAttemptsDelay")) != "") dataStoreAttemptsDelay = atoi(val.c_str());
  storeLogin = server.arg("storeLogin");
  storePassword = server.arg("storePassword");
  storeURL = server.arg("storeURL");
  iswifiConnectionCheck = server.arg("iswifiConnectionCheck")!="";
  wifiRouterIP = server.arg("wifiRouterIP");
}

