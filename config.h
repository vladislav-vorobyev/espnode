#include <ArduinoJson.h>
#include <FS.h>


class NodeConfig {
private:
  StaticJsonBuffer<1024> jsonBuffer;
public:
  String hostname; // self host name
  bool useTermoSensor1; // user 1st temperature sensor
  bool useTermoSensor2; // user 2nd temperature sensor
  bool useAlarmSensor1; // user 1st alarm sensor
  bool useAlarmSensor2; // user 2nd alarm sensor
  float t1ShiftDelta; // value to correct the t1
  float t2ShiftDelta; // value to correct the t2
  int8_t alarmInit1; // 1st alarm sensor init state value (set -1 to auto detect)
  int8_t alarmInit2; // 2nd alarm sensor init state value (set -1 to auto detect)
  bool alarmActive; // is alarm check activated
  bool alarmAutoMode; // is alarm automatic switch mode
  int alarmReadyDelay; // time to wait from start before initialize alarm pin normal state and be ready for alarm (sec)
  int alarmSkipDelay; // delay between alarm sending (sec)
  int dataStoreDelay; // delay between send data to hosting (sec)
  int dataStoreAttempts; // attempts to resending if failed
  int dataStoreAttemptsDelay; // delay between attempts to resend (sec)
  String storeLogin; // store data page access
  String storePassword;
  String storeURL; // hosting
  String storeLocalURL; // local server
  bool iswifiConnectionCheck; // is make a ping the local wifi server to recheck connection
  String wifiRouterIP; // url to check wifi
  bool tControlActive; // is temperature control relay active
  float tControlMin; // min temperature (set relay on)
  float tControlMax; // max temperature (set relay off)
  bool tAlarmActive; // is temperature alarm active
  float tAlarmMin1; // min temperature to alarm (set -127 to off)
  float tAlarmMin2; // min temperature to alarm (set -127 to off)

  NodeConfig(){
    // set default values
    hostname = "";
    useTermoSensor1 = true;
    useTermoSensor2 = true;
    useAlarmSensor1 = true;
    useAlarmSensor2 = true;
    t1ShiftDelta = t2ShiftDelta = 0;
    alarmInit1 = alarmInit2 = -1;
    alarmActive = false;
    alarmAutoMode = false;
    alarmReadyDelay = ALARM_INIT_WAIT;
    alarmSkipDelay = 0;
    dataStoreDelay = DATA_STORE_DEFAULT_DELAY;
    dataStoreAttempts = ATTEMPTS_TO_STORE_DATA;
    dataStoreAttemptsDelay = ATTEMPTS_DEFAULT_DELAY;
    storeLogin = "esp";
    storePassword = "123";
    storeURL = "";
    storeLocalURL = "";
    iswifiConnectionCheck = false;
    wifiRouterIP = "";
    tControlActive = false;
    tControlMin = 0;
    tControlMax = 0;
    tAlarmActive = false;
    tAlarmMin1 = -127;
    tAlarmMin2 = -127;
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
  read("hostname", json, hostname);
  read("useTermoSensor1", json, useTermoSensor1);
  read("useTermoSensor2", json, useTermoSensor2);
  read("useAlarmSensor1", json, useAlarmSensor1);
  read("useAlarmSensor2", json, useAlarmSensor2);
  read("t1ShiftDelta", json, t1ShiftDelta);
  read("t2ShiftDelta", json, t2ShiftDelta);
  read("alarmInit1", json, alarmInit1);
  read("alarmInit2", json, alarmInit2);
  read("alarmActive", json, alarmActive);
  read("alarmAutoMode", json, alarmAutoMode);
  read("alarmReadyDelay", json, alarmReadyDelay);
  read("alarmSkipDelay", json, alarmSkipDelay);
  read("dataStoreDelay", json, dataStoreDelay);
  read("dataStoreAttempts", json, dataStoreAttempts);
  read("dataStoreAttemptsDelay", json, dataStoreAttemptsDelay);
  read("storeLogin", json, storeLogin);
  read("storePassword", json, storePassword);
  read("storeURL", json, storeURL);
  //read("storeLocalURL", json, storeLocalURL);
  read("iswifiConnectionCheck", json, iswifiConnectionCheck);
  read("wifiRouterIP", json, wifiRouterIP);
  read("tControlActive", json, tControlActive);
  read("tControlMin", json, tControlMin);
  read("tControlMax", json, tControlMax);
  read("tAlarmActive", json, tAlarmActive);
  read("tAlarmMin1", json, tAlarmMin1);
  read("tAlarmMin2", json, tAlarmMin2);

  return true;
}

/*
* Save Config
*/
bool NodeConfig::save() {
  StaticJsonBuffer<2048> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["hostname"] = hostname;
  json["useTermoSensor1"] = useTermoSensor1;
  json["useTermoSensor2"] = useTermoSensor2;
  json["useAlarmSensor1"] = useAlarmSensor1;
  json["useAlarmSensor2"] = useAlarmSensor2;
  json["t1ShiftDelta"] = t1ShiftDelta;
  json["t2ShiftDelta"] = t2ShiftDelta;
  json["alarmInit1"] = alarmInit1;
  json["alarmInit2"] = alarmInit2;
  json["alarmActive"] = alarmActive;
  json["alarmAutoMode"] = alarmAutoMode;
  json["alarmReadyDelay"] = alarmReadyDelay;
  json["alarmSkipDelay"] = alarmSkipDelay;
  json["dataStoreDelay"] = dataStoreDelay;
  json["dataStoreAttempts"] = dataStoreAttempts;
  json["dataStoreAttemptsDelay"] = dataStoreAttemptsDelay;
  json["storeLogin"] = storeLogin;
  json["storePassword"] = storePassword;
  json["storeURL"] = storeURL;
  //json["storeLocalURL"] = storeLocalURL;
  json["iswifiConnectionCheck"] = iswifiConnectionCheck;
  json["wifiRouterIP"] = wifiRouterIP;
  json["tControlActive"] = tControlActive;
  json["tControlMin"] = tControlMin;
  json["tControlMax"] = tControlMax;
  json["tAlarmActive"] = tAlarmActive;
  json["tAlarmMin1"] = tAlarmMin1;
  json["tAlarmMin2"] = tAlarmMin2;

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
  response += "<i class='info'>(MAC&nbsp;адрес&nbsp;" + WiFi.macAddress() + ")</i><br>\n";
  response += "Сетевое имя <input type='text' name='hostname' value='" + String(hostname) + "' style='width:10em'><br>\n";
  response += "<fieldset>\n";
  response += "<legend>Активация</legend>\n";
  response += "<input type='checkbox' name='useTermoSensor1' value=1 " + String(useTermoSensor1? "checked" : "") + "> активировать 1<sup>ый</sup> сенсор температуры, скорректировать&nbsp;на&nbsp;";
  response += "<input type='text' name='t1ShiftDelta' value='" + String(t1ShiftDelta) + "' style='width:4em'> градуса<br>\n";
  response += "<input type='checkbox' name='useTermoSensor2' value=1 " + String(useTermoSensor2? "checked" : "") + "> активировать 2<sup>ой</sup> сенсор температуры, скорректировать&nbsp;на&nbsp;";
  response += "<input type='text' name='t2ShiftDelta' value='" + String(t2ShiftDelta) + "' style='width:4em'> градуса<br>\n";
  response += "<input type='checkbox' name='useAlarmSensor1' value=1 " + String(useAlarmSensor1? "checked" : "") + "> активировать 1<sup>ый</sup> сенсор тревоги,\n";
  response += "значение&nbsp;покоя&nbsp;=&nbsp;<input type='text' name='alarmInit1' value='" + ((alarmInit1 != -1)? String(alarmInit1) : "") + "' placeholder='авто' style='width:2.1em'><br>\n";
  response += "<input type='checkbox' name='useAlarmSensor2' value=1 " + String(useAlarmSensor2? "checked" : "") + "> активировать 2<sup>ой</sup> сенсор тревоги,\n";
  response += "значение&nbsp;покоя&nbsp;=&nbsp;<input type='text' name='alarmInit2' value='" + ((alarmInit2 != -1)? String(alarmInit2) : "") + "' placeholder='авто' style='width:2.1em'><br>\n";
  response += "<input type='text' name='alarmReadyDelay' value='" + String(alarmReadyDelay) + "' style='width:4em'> время ожидания готовности датчиков тревоги (сек)<br>\n";
  response += "</fieldset>\n";
  response += "<fieldset>\n";
  response += "<legend>Контроль</legend>\n";
  response += "<input type='checkbox' name='alarmActive' value=1 " + String(alarmActive? "checked" : "") + "> активировать режим тревоги<br>\n";
  response += "<input type='checkbox' name='alarmAutoMode' value=1 " + String(alarmAutoMode? "checked" : "") + "> автоматическое отключение тревоги <br>\n";
  response += "<input type='checkbox' name='tAlarmActive' value=1 " + String(tAlarmActive? "checked" : "") + "> активировать режим тревоги по температуре:<br>\n";
  response += "минимальная&nbsp;t1&nbsp;<input type='text' name='tAlarmMin1' value='" + ((tAlarmMin1 != -127)? String(tAlarmMin1) : "") + "' style='width:4em'>\n";
  response += "минимальная&nbsp;t2&nbsp;<input type='text' name='tAlarmMin2' value='" + ((tAlarmMin2 != -127)? String(tAlarmMin2) : "") + "' style='width:4em'><br>\n";
  response += "<input type='checkbox' name='tControlActive' value=1 " + String(tControlActive? "checked" : "") + "> активировать режим контроля температуры:\n";
  response += "минимум&nbsp;<input type='text' name='tControlMin' value='" + String(tControlMin) + "' style='width:4em'>\n";
  response += "максимум&nbsp;<input type='text' name='tControlMax' value='" + String(tControlMax) + "' style='width:4em'><br>\n";
  response += "</fieldset>\n";
  response += "<fieldset>\n";
  response += "<legend>Отправка данных</legend>\n";
  response += "<input type='text' name='storeURL' value='" + String(storeURL) + "' style='width:15em'> URL<br>\n";
  response += "<input type='text' name='storeLogin' value='" + String(storeLogin) + "' style='width:15em'> логин<br>\n";
  response += "<input type='text' name='storePassword' value='" + String(storePassword) + "' style='width:15em'> пароль<br>\n";
  //response += "<input type='text' name='storeLocalURL' value='" + String(storeLocalURL) + "' style='width:15em'> local server URL<br>\n";
  response += "<input type='text' name='dataStoreDelay' value='" + String(dataStoreDelay) + "' style='width:4em'> период между отправкой состояния датчиков (сек)<br>\n";
  response += "<input type='text' name='alarmSkipDelay' value='" + String(alarmSkipDelay) + "' style='width:4em'> период между повторной отправкой состояния тревоги (сек)<br>\n";
  response += "<input type='text' name='dataStoreAttempts' value='" + String(dataStoreAttempts) + "' style='width:4em'> кол-во попыток по отправке в случае неудачи<br>\n";
  response += "<input type='text' name='dataStoreAttemptsDelay' value='" + String(dataStoreAttemptsDelay) + "' style='width:4em'> период между повторными попытками по отправке (сек)<br>\n";
  response += "</fieldset>\n";
  response += "<fieldset>\n";
  response += "<legend>WiFi</legend>\n";
  response += "<input type='checkbox' name='iswifiConnectionCheck' value=1 " + String(iswifiConnectionCheck? "checked" : "") + "> проверка доступности WiFi роутера<br>\n";
  response += "<input type='text' name='wifiRouterIP' value='" + String(wifiRouterIP) + "' style='width:15em'> IP адрес WiFi роутера<br>\n";
  response += "</fieldset>\n";
  return response;
}

/*
* Handle the HTML form submit
*/
void NodeConfig::handleFormSubmit(ESP8266WebServer& server) {
  String val;
  hostname = server.arg("hostname");
  useTermoSensor1 = server.arg("useTermoSensor1")!="";
  useTermoSensor2 = server.arg("useTermoSensor2")!="";
  useAlarmSensor1 = server.arg("useAlarmSensor1")!="";
  useAlarmSensor2 = server.arg("useAlarmSensor2")!="";
  t1ShiftDelta = ((val = server.arg("t1ShiftDelta")) != "")? atof(val.c_str()) : 0;
  t2ShiftDelta = ((val = server.arg("t2ShiftDelta")) != "")? atof(val.c_str()) : 0;
  alarmInit1 = ((val = server.arg("alarmInit1")) != "")? atoi(val.c_str()) : -1;
  alarmInit2 = ((val = server.arg("alarmInit2")) != "")? atoi(val.c_str()) : -1;
  alarmActive = server.arg("alarmActive")!="";
  alarmAutoMode = server.arg("alarmAutoMode")!="";
  if ((val = server.arg("alarmReadyDelay")) != "") alarmReadyDelay = atoi(val.c_str());
  if ((val = server.arg("alarmSkipDelay")) != "") alarmSkipDelay = atoi(val.c_str());
  if ((val = server.arg("dataStoreDelay")) != "") dataStoreDelay = atoi(val.c_str());
  if ((val = server.arg("dataStoreAttempts")) != "") dataStoreAttempts = atoi(val.c_str());
  if ((val = server.arg("dataStoreAttemptsDelay")) != "") dataStoreAttemptsDelay = atoi(val.c_str());
  storeLogin = server.arg("storeLogin");
  storePassword = server.arg("storePassword");
  storeURL = server.arg("storeURL");
  //storeLocalURL = server.arg("storeLocalURL");
  iswifiConnectionCheck = server.arg("iswifiConnectionCheck")!="";
  wifiRouterIP = server.arg("wifiRouterIP");
  tControlActive = server.arg("tControlActive")!="";
  tControlMin = ((val = server.arg("tControlMin")) != "")? atof(val.c_str()) : 0;
  tControlMax = ((val = server.arg("tControlMax")) != "")? atof(val.c_str()) : 0;
  tAlarmActive = server.arg("tAlarmActive")!="";
  tAlarmMin1 = ((val = server.arg("tAlarmMin1")) != "")? atof(val.c_str()) : -127;
  tAlarmMin2 = ((val = server.arg("tAlarmMin2")) != "")? atof(val.c_str()) : -127;
}


