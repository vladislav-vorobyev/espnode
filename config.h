#include <ArduinoJson.h>
#include <FS.h>


class NodeConfig {
private:
  StaticJsonBuffer<1024> jsonBuffer;
public:
  bool alarmActive; // is alarm check activated by default
  bool alarmTestMode; // is alarm test mode
  int alarmSkipDelay; // delay between WiFi activity and alarm sensors check (sec)
  float t1ShiftDelta; // value to correct the t1
  float t2ShiftDelta; // value to correct the t2
  int dataStoreDelay; // delay between send data to hosting (sec)
  String storeLogin; // store data page access
  String storePassword;
  String storeURL; // hosting
  bool iswifiConnectionCheck; // is make a ping the local wifi server to recheck connection
  String wifiRouterIP; // url to check wifi
  String rasberyURL; // rasbery server

  NodeConfig(){
    alarmActive = false;
    alarmTestMode = false;
    alarmSkipDelay = 0;
    t1ShiftDelta = t2ShiftDelta = 0;
    dataStoreDelay = DATA_STORE_DEFAULT_DELAY;
    storeLogin = "esp8266";
    storePassword = "123";
    storeURL = "http://vvvnet.ru/home/data.php";
    iswifiConnectionCheck = false;
    wifiRouterIP = "";
    rasberyURL = "http://192.168.0.4/data.php";
  }
  bool save();
  bool load();
  String getHTMLFormFields();
  void handleFormSubmit(ESP8266WebServer& server);
};


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
  JsonVariant val;
  //
  val = json["alarmActive"];
  if (val.success()) alarmActive = val;
  Serial.println(String("alarmActive: ") + alarmActive);
  //
  val = json["alarmTestMode"];
  if (val.success()) alarmTestMode = val;
  Serial.println(String("alarmTestMode: ") + alarmTestMode);
  //
  val = json["alarmSkipDelay"];
  if (val.success()) alarmSkipDelay = val;
  Serial.println(String("alarmSkipDelay: ") + alarmSkipDelay);
  //
  val = json["dataStoreDelay"];
  if (val.success()) dataStoreDelay = val;
  Serial.println(String("dataStoreDelay: ") + dataStoreDelay);
  //
  val = json["storeLogin"];
  if (val.success()) storeLogin = val.as<String>();
  Serial.println(String("storeLogin: ") + storeLogin);
  //
  val = json["storePassword"];
  if (val.success()) storePassword = val.as<String>();
  Serial.println(String("storePassword: ") + storePassword);
  //
  val = json["storeURL"];
  if (val.success()) storeURL = val.as<String>();
  Serial.println(String("storeURL: ") + storeURL);
  //
  val = json["t1ShiftDelta"];
  if (val.success()) t1ShiftDelta = val;
  Serial.println(String("t1ShiftDelta: ") + t1ShiftDelta);
  //
  val = json["t2ShiftDelta"];
  if (val.success()) t2ShiftDelta = val;
  Serial.println(String("t2ShiftDelta: ") + t2ShiftDelta);
  //
  val = json["iswifiConnectionCheck"];
  if (val.success()) iswifiConnectionCheck = val;
  Serial.println(String("iswifiConnectionCheck: ") + iswifiConnectionCheck);
  //
  val = json["wifiRouterIP"];
  if (val.success()) wifiRouterIP = val.as<String>();
  Serial.println(String("wifiRouterIP: ") + wifiRouterIP);

  return true;
}

/*
* Save Config
*/
bool NodeConfig::save() {
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["t1ShiftDelta"] = t1ShiftDelta;
  json["t2ShiftDelta"] = t2ShiftDelta;
  json["alarmActive"] = alarmActive;
  json["alarmTestMode"] = alarmTestMode;
  json["alarmSkipDelay"] = alarmSkipDelay;
  json["dataStoreDelay"] = dataStoreDelay;
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
  response += "<input type='checkbox' name='alarmActive' value=1 " + String(alarmActive? "checked" : "") + "> set alarm active on starting<br>\n";
  response += "<input type='checkbox' name='alarmTestMode' value=1 " + String(alarmTestMode? "checked" : "") + "> alarm test mode <br>\n";
  response += "<input type='text' name='alarmSkipDelay' value='" + String(alarmSkipDelay) + "' style='width:60px'> delay between WiFi activity and alarm check (sec)<br>\n";
  response += "<input type='text' name='t1ShiftDelta' value='" + String(t1ShiftDelta) + "' style='width:60px'> t1 shift<br>\n";
  response += "<input type='text' name='t2ShiftDelta' value='" + String(t2ShiftDelta) + "' style='width:60px'> t2 shift<br>\n";
  response += "<input type='text' name='dataStoreDelay' value='" + String(dataStoreDelay) + "' style='width:60px'> store delay (sec)<br>\n";
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
  alarmActive = server.arg("alarmActive")!="";
  alarmTestMode = server.arg("alarmTestMode")!="";
  if ((val = server.arg("alarmSkipDelay")) != "") alarmSkipDelay = atoi(val.c_str());
  if ((val = server.arg("t1ShiftDelta")) != "") t1ShiftDelta = atof(val.c_str());
  if ((val = server.arg("t2ShiftDelta")) != "") t2ShiftDelta = atof(val.c_str());
  if ((val = server.arg("dataStoreDelay")) != "") dataStoreDelay = atoi(val.c_str());
  storeLogin = server.arg("storeLogin");
  storePassword = server.arg("storePassword");
  storeURL = server.arg("storeURL");
  iswifiConnectionCheck = server.arg("iswifiConnectionCheck")!="";
  wifiRouterIP = server.arg("wifiRouterIP");
}

