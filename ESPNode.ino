/*
*  ESP8266 WIFIServer Node.
*  The module to provide a temperature and alarm.
*  D1,D2,D3 - display
*  D4 (GPIO2)  - 1st temperature sensor
*  D5 (GPIO14) - 2nd temperature sensor
*  D6 (GPIO12) - gas/motion sensors
*  D7 (GPIO13) - gas/motion sensors
*  D8 (GPIO15) - buzzer
*  D0 (GPIO16)
*/

const char* SKETCH_VERSION = "0.9.9"; // sketch version

#define ONE_WIRE_BUS1 2  // DS18B20 1st sensor pin
#define ONE_WIRE_BUS2 14 // DS18B20 2nd sensor pin

#define ALARM_PIN1 12  // Alarm 1st sensor pin
#define ALARM_PIN2 13  // Alarm 2nd sensor pin
#define BUZZ_PIN 15 // Buzzer pin
#define BUZZ_TONE 1000 // Buzzer tone

#define WIFI_CONNECTING_WAIT 5 // time to wait for connecting to WiFi (sec)
#define WIFI_RECONNECT_DELAY 30 // time to wait before reconnecting to WiFi (sec)
#define ALARM_INIT_WAIT 60 // time to wait before initialize alarm pin default state (sec)

#define STORE_DATA_DELAY 300 // delay between send data to hosting (sec)


#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Ticker.h>
#include <OneWire.h>
#include <DallasTemperature.h>

Ticker storeTicker;
bool flagToStoreData = true; // up in ticker to do in the main loop

OneWire oneWire1(ONE_WIRE_BUS1);
OneWire oneWire2(ONE_WIRE_BUS2);
DallasTemperature ds18b20(&oneWire1);
DallasTemperature ds18b20_2(&oneWire2);

#include <Adafruit_GFX.h>
  #define SSD1306_LCDWIDTH                  64
  #define SSD1306_LCDHEIGHT                 48
#include <Adafruit_SSD1306.h>

class Adafruit_SSD1306_m : public Adafruit_SSD1306 {
 public:
  Adafruit_SSD1306_m(int8_t RST = -1) : Adafruit_SSD1306(RST){
  }
  inline void resetDisplay(void){
    clearDisplay();
    setTextSize(1);
    setTextColor(WHITE);
    setCursor(0,0);
  }
};
 
// SCL GPIO5 D1
// SDA GPIO4 D2
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306_m display(OLED_RESET);
 

const int activityLed = LED_BUILTIN;
const int activityOn = 0;
const int activityOff = 1;

const int wifiNetNum = 2; // number of variants in the wifiNet
const char* wifiNet[2][2] = {{"VVVHome","123"},{"VVVNetwork","123"}};
const char* ssid;
const char* password;

const char* webLogin = "user"; // firmware page access
const char* webPassword = "123";

const char* hostingURL = "http://vvvnet.ru/home/data.php"; // hosting
const char* localURL = "http://192.168.0.4/data.php"; // local server
const char* storeLogin = "user"; // store data page access
const char* storePassword = "123";

int markPos = 0; // work animation mark position
bool alarmActive = false; // alarm check activity status
bool isAlarm = false; // alarm status
bool isAlarmSent = false; // alarm sending status
bool buzzerOn = false; // buzzer on/off
int alarmInitTime, alarmInitWait; // time, delay to initialize sensors (msec)
int alarmInitState1 = -1, alarmInitState2 = -1; // alarm pins init state
int wifiReconnectDelay = 0; // delay counter before reconnecting to WiFi

bool isExistsWire1 = false;
bool isExistsWire2 = false;

ESP8266WebServer server(80);

/*
* Starting the node
*/
void setup(void){
  Serial.begin(115200);
  Serial.println("Begin");
  Serial.print("Firmware version ");
  Serial.println(SKETCH_VERSION);

  // setup time to initialize sensors (msec)
  alarmInitTime = millis() + ALARM_INIT_WAIT * 1000;

  // Prepare dispay
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  // init done
  Serial.println("Display init done.");
 
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(500);
 
  // Clear the display.
  display.resetDisplay();
  display.println("Firmware:");
  display.println(SKETCH_VERSION);
 
  // text display tests
  display.display();
  delay(1500);


  // Prepare sensors
  Serial.println("Sensors:");
  display.println("Sensors:");
  display.display();

  // termo
  if (oneWire2.reset()) {
    isExistsWire2 = true;
    ds18b20_2.begin();
    Serial.println("DS18B20 2 init done.");
    display.print("t2 ");
  } else {
    Serial.println("DS18B20 2 - no sensor.");
    display.print("-- ");
  }
  if (oneWire1.reset()) {
    isExistsWire1 = true;
    ds18b20.begin();
    Serial.println("DS18B20 1 init done.");
    display.print("t1 ");
  } else {
    Serial.println("DS18B20 1 - no sensor.");
    display.print("-- ");
  }
  display.println("");
  display.display();

  // initialize the alarm pin as an input:
  pinMode(ALARM_PIN1, INPUT);
  pinMode(ALARM_PIN2, INPUT);
  Serial.println("alarm pins init done.");
  display.println("Alarm ok");
  display.display();

  // Prepare activity led
  pinMode(activityLed, OUTPUT);
  digitalWrite(activityLed, activityOn);
  Serial.println("activity led init done.");
  display.println("Led ok");
  display.display();
  delay(500);

  // Connect to WiFi network
  if (!connectToWiFi()) {
    wifiReconnectDelay = WIFI_RECONNECT_DELAY * 2;
    delay(1000);
  }
  
  // Start the server
  initWebServer();
  initWebUpdate();
  server.begin();
  Serial.println("HTTP server started");
  display.println("HTTP server started");
  display.display();

  // test run of the json handler
  Serial.println("Test handle JSON:");
  handleJSON();
  
  // init ticker
  storeTicker.attach(STORE_DATA_DELAY, [](){
    flagToStoreData = true;
  });
  
  Serial.println("Initialization finished.");
  digitalWrite(activityLed, activityOff);
}

/*
* Main loop
*/
void loop(void){
  //watchdog
  ESP.wdtDisable(); //Кормление сторожевого пса

  //wifi check
  if (wifiReconnectDelay > 0)
    wifiReconnectDelay--;
  if (WiFi.status() != WL_CONNECTED && wifiReconnectDelay == 0) {
    // try to reconnect
    if (!connectToWiFi()) {
      wifiReconnectDelay = WIFI_RECONNECT_DELAY * 2;
      delay(1000);
    }
  }
  
  server.handleClient();

  // temperature
  String temperature = getTemperature(&ds18b20);

  // read the state of the alarm sensors
  int alarm1 = digitalRead(ALARM_PIN1);
  int alarm2 = digitalRead(ALARM_PIN2);
  
  // alarm pins initialize
  alarmInitWait = alarmInitTime - millis(); // delay before initialize sensors (msec)
  if (alarmInitWait <= 0) {
    alarmInitWait = 0;
    if (alarmInitState1 < 0)
      alarmInitState1 = alarm1;
    if (alarmInitState2 < 0)
      alarmInitState2 = alarm2;
  }
  
  // alarm check
  if (alarmActive && alarmInitWait <= 0) {
    if (alarm1 != alarmInitState1 || alarm2 != alarmInitState2) {
      isAlarm = true;
    }
  }

  // display
  display.resetDisplay();
  display.println(WiFi.localIP());
  display.print(String(alarm1) + " " + String(alarm2));
  if (alarmInitWait <= 0)
    display.print(" R");
  else
    display.print(String(" ") + String(alarmInitWait));
  if (alarmActive)
    display.print(" A");
  if (!isAlarm) {
    // show 1st sensor temperature
    display.setCursor(0,28);
    display.setTextSize(2);
    display.println(isExistsWire1? temperature : "--");
    // busser set off
    if (buzzerOn) {
      noTone(BUZZ_PIN);
      buzzerOn = false;
    }
  } else {
    // alarm
    if (markPos % 2 == 0) {
      display.setCursor(0,28);
      display.setTextSize(2);
      display.println("ALARM");
    }
    // buzzer activity
    if (buzzerOn) {
      noTone(BUZZ_PIN);
      buzzerOn = false;
    } else {
      tone(BUZZ_PIN, BUZZ_TONE);
      buzzerOn = true;
    }
  }
  
  // animation to show activity
  int pos = 4 + markPos * 6;
  display.drawPixel(pos, 47, WHITE);
  display.drawPixel(pos+1, 47, WHITE);
  display.drawPixel(pos+2, 47, WHITE);
  if (++markPos > 9) markPos = 0;
  display.display();

  // send alarm
  if (isAlarm) {
    if (!isAlarmSent) {
      isAlarmSent = storeData(hostingURL);
    }
  }
  
  // send data to hosting
  if (flagToStoreData) {
    flagToStoreData = false;
    if (storeData(hostingURL))
      Serial.println("Stored.");
  }

  delay(500);
}



/*
* Try to connect to WiFi
*/
bool connectToWiFi() {
  //display
  Serial.println();
  Serial.println("Try to connect to:");
  for (int k = 0; k < wifiNetNum; k++)
    Serial.println(wifiNet[k][0]);
  Serial.println("Scan WiFi networks");
  display.resetDisplay();
  display.println("Scan WiFi");
  display.display();
  
  // start WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  bool ssid_exists = false;
  if (n == 0) {
    Serial.println("no networks found");
    display.println("no networks found");
    display.display();
    return false;
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      // check
      String id = WiFi.SSID(i);
      for (int k = 0; k < wifiNetNum; k++) {
        if (id == String(wifiNet[k][0])) {
          ssid_exists = true;
          ssid = wifiNet[k][0];
          password = wifiNet[k][1];
        }
      }
      delay(10);
    }
    if (!ssid_exists) {
      Serial.println("Not found.");
      display.println(wifiNet[0][0]);
      display.println("Not found!");
      display.display();
      return false;
    }
  }

  // display
  Serial.println();
  Serial.println();
  Serial.print("Connecting to " + String(ssid));
  display.println(ssid);
  display.println("Connecting");
  display.display();

  // start connection
  WiFi.begin(ssid, password);

  // connecting to WiFi with indication
  int wait = WIFI_CONNECTING_WAIT * 2;
  int line = 4;
  int col = 1;
  while (WiFi.status() != WL_CONNECTED && wait > 0) {
    // animate connection process
    if (col > 10) {
      col = 1;
      line++;
    }
    if (line > 6) {
      line = 1;
      display.clearDisplay();
      display.setCursor(0,0);
    }
    Serial.print(".");
    display.print(".");
    display.display();
    col++;
    wait--;
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // connected
    Serial.println("");
    Serial.println(String("Connected to ") + ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    display.resetDisplay();
    display.println("Connected.");
    display.println(WiFi.localIP());
    display.display();
  
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    return true;
  } else {
    Serial.println("");
    Serial.println(String("Can not connect to ") + ssid);
    display.resetDisplay();
    display.println("Not connected.");
    return false;
  }
}

/*
* Get temperature from sensor
*/
String getTemperature(DallasTemperature *ds18b20){
  // Polls the sensors
  ds18b20->requestTemperatures();
  // Gets first probe on wire in lieu of by address
  float temperature = ds18b20->getTempCByIndex(0);
  return(String(temperature));
}


/*
* Prepare JSON
*/
String getJSON(){
  String mac = WiFi.macAddress();
  String temperature1 = (isExistsWire1)? getTemperature(&ds18b20) : "";
  String temperature2 = (isExistsWire2)? getTemperature(&ds18b20_2) : "";
  String s_alarm = (isAlarm)? "true" : "false";
  String s_alarm1 = String(digitalRead(ALARM_PIN1));
  String s_alarm2 = String(digitalRead(ALARM_PIN2));
  String s_alarm_init1 = String(alarmInitState1);
  String s_alarm_init2 = String(alarmInitState2);
  String s_alarm_ready = (alarmInitWait <= 0)? "true" : "false";
  String s_alarm_active = (alarmActive)? "true" : "false";
  String s_alarm_wait = String(alarmInitWait);
  
  Serial.println();
  Serial.println("Mac address: " + mac);
  Serial.println("Temperature1: " + temperature1);
  Serial.println("Temperature2: " + temperature2);
  Serial.println("Alarm1: " + s_alarm1);
  Serial.println("Alarm2: " + s_alarm2);
  Serial.println("AlarmDefaultState1: " + s_alarm_init1);
  Serial.println("AlarmDefaultState2: " + s_alarm_init2);
  if (alarmInitWait > 0)
    Serial.println("AlarmWait: " + s_alarm_wait);
  Serial.println("AlarmReady: " + s_alarm_ready);
  Serial.println("AlarmActive: " + s_alarm_active);
  Serial.println("Alarm: " + s_alarm);
  
  String json = "{";
  json += "\"t1\":\"" + temperature1 + "\", ";
  json += "\"t2\":\"" + temperature2 + "\", ";
  json += "\"alarm1\":" + s_alarm1 + ", ";
  json += "\"alarm2\":" + s_alarm2 + ", ";
  json += "\"alarm_init1\":" + s_alarm_init1 + ", ";
  json += "\"alarm_init2\":" + s_alarm_init2 + ", ";
  json += "\"alarm_wait\":" + s_alarm_wait + ", ";
  json += "\"alarm_ready\":" + s_alarm_ready + ", ";
  json += "\"alarm_active\":" + s_alarm_active + ", ";
  json += "\"alarm\":" + s_alarm + ", ";
  json += "\"mac\":\"" + mac + "\", ";
  json += "\"ver\":\"" + String(SKETCH_VERSION) + "\"";
  json += "}";

  return json;
}


/*
* Send data to the hosting
*/
bool storeData(const char* URL){
  HTTPClient http;
  String post_data = String("data=") + getJSON();
  
  Serial.println();
  Serial.println("[HTTP] begin...");
  http.begin(URL);
  http.setAuthorization(storeLogin, storePassword);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(post_data);
  http.writeToStream(&Serial);
  http.end();
  Serial.println("[HTTP] end");
  return (httpCode == 200);
}

/*
* server.on("/inline", [](){
* server.send(200, "text/plain", "this works as well");
* });
*/
void initWebServer() {
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/json", handleJSON);
  server.on("/mac", HTTP_GET, [](){
    digitalWrite(activityLed, activityOn);
    serverSendHeaders();
    server.send(200, "text/plain", WiFi.macAddress());
    digitalWrite(activityLed, activityOff);
  });
  server.on("/restart", HTTP_GET, [](){
    digitalWrite(activityLed, activityOn);
    if(!server.authenticate(webLogin, webPassword))
      return server.requestAuthentication();
    serverSendHeaders();
    server.send(200, "text/plain", "Restart...");
    ESP.restart();
    digitalWrite(activityLed, activityOff);
  });
  server.on("/alarm", HTTP_GET, [](){
    digitalWrite(activityLed, activityOn);
    String response = String(isAlarm?"1":"0");
    // get arguments
    String set = server.arg("set");
    int wait = atoi(server.arg("wait").c_str());
    // change status
    if (set == "on") {
      alarmActive = true;
      isAlarm = false;
      isAlarmSent = false;
      if (wait > 0) {
        alarmInitTime = millis() + wait * 1000;
        alarmInitState1 = -1; // alarm pins init state
        alarmInitState2 = -1;
      }
    }
    if (set == "off") {
      alarmActive = false;
      isAlarm = false;
      isAlarmSent = false;
    }
    if (set == "test") {
      alarmActive = true;
      isAlarm = true;
    }
    // response
    serverSendHeaders();
    server.send(200, "text/plain", response);
    digitalWrite(activityLed, activityOff);
  });
}

void serverSendHeaders() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
}

void handleJSON() {
  digitalWrite(activityLed, activityOn);

  String json = getJSON();
  serverSendHeaders();
  server.send(200, "application/json", json);
  
  digitalWrite(activityLed, activityOff);
}

void handleRoot() {
  digitalWrite(activityLed, activityOn);
  
  String temperature1 = getTemperature(&ds18b20);
  String temperature2 = getTemperature(&ds18b20_2);
  
  Serial.println();
  Serial.println("Temperature1: " + temperature1);
  Serial.println("Temperature2: " + temperature2);
  
  serverSendHeaders();
  String response = "<!DOCTYPE HTML>\r\n<html>\r\n<head>\r\n";
  response += "<meta http-equiv='Cache-control' content='no-cache'>\r\n";
  response += "<style>@media screen and (max-width: 1200px){h1{font-size:14em}h2{font-size:5em}}</style>\r\n";
  response += "</head>\r\n<body>\r\n";
  response += "<div style='text-align:center'><h2>Temperature1:</h2><h1>" + temperature1 + "</h1></div>\r\n";
  response += "<div style='text-align:center'><h2>Temperature2:</h2><h1>" + temperature2 + "</h1></div>\r\n";
  response += "</body>\r\n</html>\r\n";
  server.send(200, "text/html", response);
  
  digitalWrite(activityLed, activityOff);
}

void handleNotFound(){
  digitalWrite(activityLed, activityOn);
  
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  
  digitalWrite(activityLed, activityOff);
}


void initWebUpdate(){
  server.on("/firmware", HTTP_GET, [](){
    if(!server.authenticate(webLogin, webPassword))
      return server.requestAuthentication();
    
    serverSendHeaders();
    server.send(200, "text/html", "<form method='POST' action='/firmware-update' enctype='multipart/form-data'><input type='file' name='update'> <input type='submit' value='Update'></form>");
  });
  server.on("/firmware-update", HTTP_POST, [](){
    serverSendHeaders();
    if (Update.hasError()) {
      server.send(200, "text/plain", "FAIL");
    } else {
      server.send(200, "text/plain", "OK");
      ESP.restart();
    }
  },[](){
    if(!server.authenticate(webLogin, webPassword))
      return false;
    
    // upload
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START){
      Serial.setDebugOutput(true);
      //WiFiUDP::stopAll();
      Serial.println();
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if(!Update.begin(maxSketchSpace)){//start with max available size
        Update.printError(Serial);
      }
    } else if(upload.status == UPLOAD_FILE_WRITE){
      if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
        Update.printError(Serial);
      }
    } else if(upload.status == UPLOAD_FILE_END){
      if(Update.end(true)){ //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
}

