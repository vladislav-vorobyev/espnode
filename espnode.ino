/*
*  ESP8266 WIFIServer Node.
*  Module to provide temperature and alarm.
*  D1,D2,D3 - display
*  D4 (GPIO2)  - 1st temperature sensor
*  D5 (GPIO14) - 2nd temperature sensor
*  D6 (GPIO12) - gas/motion sensors
*  D7 (GPIO13) - gas/motion sensors
*  D8 (GPIO15) - buzzer
*  D0 (GPIO16)
*
*  4 pin sensor connector
*   ||_|
*   -+ d
*/

const char* SKETCH_VERSION = "0.9.24"; // sketch version

#define ONE_WIRE_BUS1 2  // DS18B20 1st sensor pin
#define ONE_WIRE_BUS2 14 // DS18B20 2nd sensor pin

#define ALARM_PIN1 12  // Alarm 1st sensor pin
#define ALARM_PIN2 13  // Alarm 2nd sensor pin
#define BUZZ_PIN 15 // Buzzer pin
#define BUZZ_TONE 1000 // Buzzer tone

#define WIFI_CONNECTING_WAIT 10 // time to wait for connecting to WiFi (sec)
#define WIFI_RECONNECT_DELAY 30 // time to wait before reconnecting to WiFi (sec)
#define WIFI_CONNECTION_CHECK_DELAY 60 // delay between connection check (sec)
#define ALARM_INIT_WAIT 60 // default time to wait from start before initialize alarm pin default state (sec)
#define DATA_STORE_DEFAULT_DELAY 300 // default delay between send data to hosting (sec)
#define ATTEMPTS_TO_STORE_DATA 20 // default number of attempts to store data
#define ATTEMPTS_DEFAULT_DELAY 15 // default delay between attempts to store data (sec)

#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Ticker.h>
#include <OneWire.h>
#include <DallasTemperature.h>

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
#define OLED_RESET 0  // GPIO0 D3
Adafruit_SSD1306_m display(OLED_RESET);

#include "config.h"
NodeConfig config;

#include "access.h"
const char* ssid;
const char* password;

const int activityLed = LED_BUILTIN;
const int activityOn = 0;
const int activityOff = 1;

unsigned long lastMillisValue = 0; // preserve value to catch overflow
int markPos = 0; // work animation mark position
bool isAlarm = false; // alarm status
bool isAlarmSent = false; // alarm sending status
bool buzzerOn = false; // buzzer on/off
unsigned long alarmInitTime, alarmInitWait; // time, delay to initialize sensors (msec)
int alarmInitState1 = -1, alarmInitState2 = -1; // alarm pins init state (loaded from config)
unsigned long wifiReconnectTime = 0; // next time to reconnecting to WiFi (msec)

bool isExistsWire1 = false;
bool isExistsWire2 = false;

Ticker storeTicker, storeAttemptsTicker;
bool tryToStoreData = true; // up in ticker to do in the main loop
int attemptsToStoreData = ATTEMPTS_TO_STORE_DATA; // attempts counter (loaded from config)

Ticker connectionTicker;
bool tryToCheckConnection = false; // up in ticker to do in the main loop

Ticker wifiActivityTicker;
bool skipAlarmCheck = false; // up in sending/receiving func and off in the ticker

OneWire oneWire1(ONE_WIRE_BUS1);
OneWire oneWire2(ONE_WIRE_BUS2);
DallasTemperature ds18b20_1(&oneWire1);
DallasTemperature ds18b20_2(&oneWire2);

ESP8266WebServer server(80);


/*
* Starting the node
*/
void setup(void){
  Serial.begin(115200);
  Serial.println("Begin");
  Serial.print("Firmware version ");
  Serial.println(SKETCH_VERSION);

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


  // Load config
  Serial.println("Mounting FS...");
  display.println("Mounting..");
  display.display();
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    display.println("FS failed!");
  } else {
    if (!config.load()) {
      Serial.println("Failed to load config");
      display.println("Failed to load config");
    } else {
      Serial.println("Config loaded");
      display.println("Config OK");
    }
  }
  display.display();
  delay(500);


  // Prepare sensors
  Serial.println("Sensors:");
  display.resetDisplay();
  display.println("Sensors:");
  display.display();

  // termo
  display.print("T: ");
  if (oneWire1.reset()) {
    isExistsWire1 = true;
    ds18b20_1.begin();
    Serial.println("DS18B20 1 init done.");
    display.print("1 ");
    if (!config.useTermoSensor1) {
      Serial.println("DS18B20 1 - do not use.");
    }
  } else {
    Serial.println("DS18B20 1 - no sensor.");
    display.print("n ");
  }
  if (oneWire2.reset()) {
    isExistsWire2 = true;
    ds18b20_2.begin();
    Serial.println("DS18B20 2 init done.");
    display.print("2 ");
    if (!config.useTermoSensor2) {
      Serial.println("DS18B20 2 - do not use.");
    }
  } else {
    Serial.println("DS18B20 2 - no sensor.");
    display.print("n ");
  }
  display.println("ok");
  display.display();

  // initialize the alarm pin as an input:
  pinMode(ALARM_PIN1, INPUT);
  pinMode(ALARM_PIN2, INPUT);
  display.print("A:");
  if (config.useAlarmSensor1 && config.alarmInit1 != -1) display.print(" ");
  if (config.useAlarmSensor1) {
    display.print(String(config.alarmInit1) + " ");
  } else {
    Serial.println("alarm 1 - do not use.");
    display.print("- ");
  }
  if (config.useAlarmSensor2) {
    display.print(String(config.alarmInit2) + " ");
  } else {
    Serial.println("alarm 2 - do not use.");
    display.print("- ");
  }
  Serial.println("alarm pins init done.");
  display.println("ok");
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
    wifiReconnectTime = millis() + WIFI_RECONNECT_DELAY * 1000;
    delay(1000);
  }
  
  // Start the server
  initWebServer();
  initWebUpdate();
  server.begin();
  Serial.println("HTTP server started");
  display.println("HTTP server started");
  display.display();

  // init from config
  Serial.println();
  Serial.println("Init from config.");
  initFromConfig();
  
  // test run of get json
  Serial.println();
  Serial.print("Test JSON:");
  getJSON();
  
  Serial.println();
  Serial.println("Initialization finished.");
  digitalWrite(activityLed, activityOff);
}


/*
* Main loop
*/
void loop(void){
  //reset hardware watchdog (~6 sec)
  ESP.wdtDisable();

  // uptime limit ~50 days
  // restart device if the milliseconds overflow reached
  if (millis() < lastMillisValue) {
    Serial.println("Restarting due the milliseconds limit");
    ESP.restart();
  }
  lastMillisValue = millis();

  //wifi check
  if (WiFi.status() == WL_CONNECTED && tryToCheckConnection) {
    tryToCheckConnection = false;
    if (!checkConnection(config.wifiRouterIP.c_str())) {
      WiFi.disconnect();
    }
  }
  if (WiFi.status() != WL_CONNECTED && wifiReconnectTime < millis()) {
    // try to reconnect
    if (!connectToWiFi()) {
      wifiReconnectTime = millis() + WIFI_RECONNECT_DELAY * 1000;
      delay(1000);
    }
  }
  
  server.handleClient();

  // temperature
  String temperature1 = (isExistsWire1 && config.useTermoSensor1)? getTemperature(&ds18b20_1, config.t1ShiftDelta) : "--";
  String temperature2 = (isExistsWire2 && config.useTermoSensor2)? getTemperature(&ds18b20_2, config.t1ShiftDelta) : "--";

  // read the state of the alarm sensors
  int alarm1 = config.useAlarmSensor1? digitalRead(ALARM_PIN1) : alarmInitState1;
  int alarm2 = config.useAlarmSensor2? digitalRead(ALARM_PIN2) : alarmInitState2;
  String a1 = config.useAlarmSensor1? String(alarm1) : "-";
  String a2 = config.useAlarmSensor2? String(alarm2) : "-";
  
  // alarm pins initialize
  if (alarmInitTime <= millis()) {
    alarmInitWait = 0;
    if (alarmInitState1 < 0)
      alarmInitState1 = alarm1;
    if (alarmInitState2 < 0)
      alarmInitState2 = alarm2;
  } else {
    alarmInitWait = alarmInitTime - millis(); // delay before initialize sensors (msec)
  }
  
  // alarm check
  if (config.alarmActive && alarmInitWait <= 0 && (!skipAlarmCheck || config.alarmSkipDelay == 0)) {
    if (alarm1 != alarmInitState1 || alarm2 != alarmInitState2) {
      isAlarm = true;
    }
  }

  // display
  display.resetDisplay();
  display.println(WiFi.localIP());
  display.print(a1 + " " + a2);
  if (alarmInitWait <= 0)
    display.print(" R");
  else
    display.print(String(" ") + String(alarmInitWait));
  if (config.alarmActive)
    display.print(" A");
  else
    if (alarmInitWait <= 0 && isExistsWire1 && config.useTermoSensor1 && isExistsWire2 && config.useTermoSensor2)
      display.print(temperature2);
  if (!isAlarm) {
    // show 1st sensor temperature
    display.setCursor(0,28);
    display.setTextSize(2);
    display.println((isExistsWire1 && config.useTermoSensor1)? temperature1 : temperature2);
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

  // store data
  if (WiFi.status() == WL_CONNECTED) {
    // enable soft watchdog to avoid hardware reset in 6 sec
    ESP.wdtEnable(100000);
    ESP.wdtFeed();
    
    // send alarm
    if (isAlarm) {
      if (!isAlarmSent) {
        isAlarmSent = storeData(config.storeURL.c_str());
        Serial.println("Alarm sent: " + String(isAlarmSent));
      }
      if (isAlarmSent && config.alarmTestMode && alarm1 == alarmInitState1 && alarm2 == alarmInitState2) {
        Serial.println("Alarm reset to off");
        isAlarm = isAlarmSent = false;
        // send off status to store
        //initDataStoreProcedure(5);
      }
    }
    
    // send data to hosting
    if (tryToStoreData) {
      tryToStoreData = false;
      if (storeData(config.storeURL.c_str())) {
        attemptsToStoreData = 0;
        Serial.println("Stored.");
      } else {
        attemptsToStoreData--;
      }
    }

    // switch back to hardware watchdog
    ESP.wdtDisable();
    
  } else {
    if (isAlarm && config.alarmTestMode) {
      Serial.println("Alarm reset to off");
      isAlarm = isAlarmSent = false;
    }
  }

  delay(500);
}



/*
* Initialize from config
*/
void initFromConfig(){
  // init store ticker
  storeTicker.attach(config.dataStoreDelay, [](){
    initDataStoreProcedure(config.dataStoreAttempts);
  });
  // init connection ticker
  if (config.iswifiConnectionCheck)
    connectionTicker.attach(WIFI_CONNECTION_CHECK_DELAY, [](){
      tryToCheckConnection = true;
    });
  else
    connectionTicker.detach();
  // init alarm skip ticker
  if (config.alarmSkipDelay > 0)
    wifiActivityTicker.attach(config.alarmSkipDelay, [](){
      skipAlarmCheck = false; // up in sending/receiving func and off in the ticker
    });
  else
    wifiActivityTicker.detach();
  // alarm init state
  alarmInitState1 = config.alarmInit1;
  alarmInitState2 = config.alarmInit2;
  isAlarm = isAlarmSent = false;
  // setup time to initialize sensors (msec)
  alarmInitTime = millis() + config.alarmReadyDelay * 1000;
}

/*
* Initialize data sending ticker and flags
*/
void initDataStoreProcedure(int attempts){
  attemptsToStoreData = attempts;
  tryToStoreData = true;
  // init attempts ticker
  storeAttemptsTicker.attach(config.dataStoreAttemptsDelay, [](){
    if (attemptsToStoreData > 0)
      tryToStoreData = true;
    else
      storeAttemptsTicker.detach();
  });
}


/*
* Try to connect to WiFi
*/
bool connectToWiFi() {
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  
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
    
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
 
    //reset hardware watchdog (~6 sec) to avoid restart
    ESP.wdtDisable();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // connected
    Serial.println();
    Serial.println(String("Connected to ") + ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    display.resetDisplay();
    display.println("Connected.");
    display.println(WiFi.localIP());
    display.display();
  
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    
    wifiReconnectTime = 0;
    return true;
    
  } else {
    Serial.println();
    Serial.println(String("Can not connect to ") + ssid);
    display.resetDisplay();
    display.println("Not connected.");
    return false;
  }
}

/*
* Get temperature from sensor
*/
String getTemperature(DallasTemperature *ds18b20_1, float shift){
  // Polls the sensors
  ds18b20_1->requestTemperatures();
  // Gets first probe on wire in lieu of by address
  float temperature = ds18b20_1->getTempCByIndex(0) + shift;
  return(String(temperature));
}


/*
* Get uptime as string
*/
String getUptime(){
  unsigned long msec = millis();
  int days = int(msec / (1000*60*60*24));
  msec = msec % (1000*60*60*24);
  int hours = int(msec / (1000*60*60));
  msec = msec % (1000*60*60);
  int minutes = int(msec / (1000*60));
  msec = msec % (1000*60);
  char uptime[50];
  sprintf(uptime, "%d days %02d:%02d:%02d", days, hours, minutes, msec/1000);
  return(String(uptime));
}


/*
* Prepare JSON
*/
String getJSON(){
  String mac = WiFi.macAddress();
  String temperature1 = (isExistsWire1 && config.useTermoSensor1)? getTemperature(&ds18b20_1, config.t1ShiftDelta) : "";
  String temperature2 = (isExistsWire2 && config.useTermoSensor2)? getTemperature(&ds18b20_2, config.t2ShiftDelta) : "";
  String s_alarm = (isAlarm)? "true" : "false";
  String s_alarm1 = (config.useAlarmSensor1)? String(digitalRead(ALARM_PIN1)) : "-1";
  String s_alarm2 = (config.useAlarmSensor2)? String(digitalRead(ALARM_PIN2)) : "-1";
  String s_alarm_init1 = String(alarmInitState1);
  String s_alarm_init2 = String(alarmInitState2);
  String s_alarm_active = (config.alarmActive)? "true" : "false";
  String s_alarm_wait = String(alarmInitWait);
  
  Serial.println();
  Serial.println("Mac address: " + mac);
  Serial.println("Temperature1: " + temperature1);
  Serial.println("Temperature2: " + temperature2);
  Serial.println("Alarm1: " + s_alarm1);
  Serial.println("Alarm2: " + s_alarm2);
  Serial.println("AlarmDefaultState1: " + s_alarm_init1);
  Serial.println("AlarmDefaultState2: " + s_alarm_init2);
  Serial.println("AlarmWait: " + s_alarm_wait);
  Serial.println("config.alarmActive: " + s_alarm_active);
  Serial.println("Alarm: " + s_alarm);
  
  String json = "{";
  json += "\"t1\":\"" + temperature1 + "\", ";
  json += "\"t2\":\"" + temperature2 + "\", ";
  json += "\"alarm1\":" + s_alarm1 + ", ";
  json += "\"alarm2\":" + s_alarm2 + ", ";
  json += "\"alarm_init1\":" + s_alarm_init1 + ", ";
  json += "\"alarm_init2\":" + s_alarm_init2 + ", ";
  json += "\"alarm_wait\":" + s_alarm_wait + ", ";
  json += "\"alarm_active\":" + s_alarm_active + ", ";
  json += "\"alarm\":" + s_alarm + ", ";
  json += "\"mac\":\"" + mac + "\", ";
  json += "\"uptime\":\"" + getUptime() + "\", ";
  json += "\"ver\":\"" + String(SKETCH_VERSION) + "\"";
  json += "}";

  return json;
}


/*
* Send data to the hosting
*/
bool storeData(const char* URL){
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker

  HTTPClient http;
  String post_data = String("data=") + getJSON();

  Serial.println();
  Serial.println("[HTTP] begin...");
  int httpCode = 0;
  if (http.begin(URL)) {
    http.setAuthorization(config.storeLogin.c_str(), config.storePassword.c_str());
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    httpCode = http.POST(post_data);
    if (httpCode != 200)
      Serial.println(httpCode);
    http.writeToStream(&Serial);
    http.end();
  } else {
    Serial.println("[HTTP] fail");
  }
  Serial.println("[HTTP] end");

  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker

  return (httpCode == 200);
}

/*
* Check server availability
*/
bool checkConnection(const char* host){
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
    return false;
  }
  
  // send the request to the server
  client.print("HEAD / HTTP/1.1\r\nConnection: close\r\n\r\n");
  delay(10);
  
  // Read all the lines of the reply from server and print them to Serial
  //while(client.available()){
  //  String line = client.readStringUntil('\r');
  //  Serial.print(line);
  //}
  Serial.print(client.readStringUntil('\r'));
  
  Serial.println("<end>");
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  return true;
}

/*
* server.on("/inline", [](){
* server.send(200, "text/plain", "this works as well");
* });
*/
void initWebServer() {
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  
  // json
  server.on("/json", [](){
    serverSendHeaders();
    server.send(200, "application/json", getJSON());
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  });
  
  // mac
  server.on("/mac", HTTP_GET, [](){
    serverSendHeaders();
    server.send(200, "text/plain", WiFi.macAddress());
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  });
  
  // uptime
  server.on("/uptime", HTTP_GET, [](){
    serverSendHeaders();
    server.send(200, "text/plain", getUptime());
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  });
  
  // restart
  server.on("/restart", HTTP_GET, [](){
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
    if(!server.authenticate(webLogin, webPassword))
      return server.requestAuthentication();
    serverSendHeaders();
    server.send(200, "text/plain", "Restart...");
    ESP.restart();
  });
  
  // alarm
  server.on("/alarm", HTTP_GET, [](){
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
    if(!server.authenticate(webLogin, webPassword))
      return server.requestAuthentication();
    serverSendHeaders();
    server.send(200, "text/html", htmlHeader() + "<form method='POST' action='/alarm-update'>\n"
      + (config.useAlarmSensor1? (String("Use 1<sup>st</sup> alarm sensor with normal state = ") + ((config.alarmInit1 != -1)? String(config.alarmInit1) : "auto detect") + "<br>\n") : "")
      + (config.useAlarmSensor2? (String("Use 2<sup>nd</sup> alarm sensor with normal state = ") + ((config.alarmInit2 != -1)? String(config.alarmInit2) : "auto detect") + "<br>\n") : "")
      + (config.alarmActive? "" : "Delay before activation <input type='text' name='wait' value='300' placeholder='' style='width:40px'> (sec)<br>\n")
      + "<input name='submit' type='submit' value='" + (config.alarmActive? "Deactivate" : "Activate") + "' style='margin-top:15px'>"
      + "</form>"
    );
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  });
 
  // alarm-update
  server.on("/alarm-update", HTTP_POST, [](){
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
    if(!server.authenticate(webLogin, webPassword))
      return server.requestAuthentication();
    // handle submit
    String response = "";
    if (server.arg("submit") == "Activate") {
      int wait = atoi(server.arg("wait").c_str());
      if (wait > 0) {
        alarmInitTime = millis() + wait * 1000;
      }
      config.alarmActive = true;
      alarmInitState1 = config.alarmInit1; // alarm pins init state
      alarmInitState2 = config.alarmInit2;
      isAlarm = isAlarmSent = false;
      Serial.println("Alarm mode activated");
      response += "Alarm mode activated\n";
    } else { // Deactivate
      config.alarmActive = false;
      isAlarm = isAlarmSent = false;
      Serial.println("Alarm deactivated");
      response += "Alarm deactivated\n";
    }
    // save
    Serial.println();
    if (!config.save()) {
      Serial.println("Failed to save config");
      response += "Failed to save config\n";
    } else {
      Serial.println("Config saved");
      response += "Config saved\n";
    }
    // response
    serverSendHeaders();
    server.send(200, "text/plain", response);
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  });

  // config
  server.on("/config", HTTP_GET, [](){
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
    if(!server.authenticate(webLogin, webPassword))
      return server.requestAuthentication();
    serverSendHeaders();
    server.send(200, "text/html", htmlHeader() + "<form method='POST' action='/config-save'>\n" + config.getHTMLFormFields() + "<input type='submit' value='Save' style='margin-top:15px'></form>");
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  });
 
  // config-save
  server.on("/config-save", HTTP_POST, [](){
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
    if(!server.authenticate(webLogin, webPassword))
      return server.requestAuthentication();
    // handle submit
    config.handleFormSubmit(server);
    // save
    String response = "";
    Serial.println();
    if (!config.save()) {
      Serial.println("Failed to save config");
      response = "Failed to save config\n";
    } else {
      Serial.println("Config saved");
      response = "Config saved\n";
    }
    // reinit
    initFromConfig();
    // response
    serverSendHeaders();
    server.send(200, "text/plain", response);
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  });
}

void serverSendHeaders() {
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  server.sendHeader("Connection", "close");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
}

String htmlHeader() {
  return String("<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Transitional//EN' 'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'>\r\n")
    + "<html xmlns='http://www.w3.org/1999/xhtml'>\r\n<head>\r\n"
    + "<meta http-equiv='Cache-control' content='no-cache'>\r\n"
    + "<meta http-equiv='Content-Type' content='text/html; charset=utf-8' />\r\n"
    + "<style>body {font-family:sans-serif; font-size:90%; line-height:1.6; color:#555;} input[type=\"text\" i] {padding:1px 4px; border:1px solid #ccc;}</style>\r\n"
    + "\r\n"
    + "</head>\r\n<body>\r\n"
    + "<i style='color:#aaa; font-size:90%;'>Firmware version: " + String(SKETCH_VERSION) + "</i><br>\r\n";
}

void handleRoot() {
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
  
  String temperature1 = getTemperature(&ds18b20_1, config.t1ShiftDelta);
  String temperature2 = getTemperature(&ds18b20_2, config.t2ShiftDelta);
  
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
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
}

void handleNotFound(){
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
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
  skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
}


void initWebUpdate(){
  server.on("/firmware", HTTP_GET, [](){
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
    if(!server.authenticate(webLogin, webPassword))
      return server.requestAuthentication();
    
    serverSendHeaders();
    server.send(200, "text/html", "<form method='POST' action='/firmware-update' enctype='multipart/form-data'><input type='file' name='update'> <input type='submit' value='Update'></form>");
    skipAlarmCheck = true; // up in sending/receiving func and off in the ticker
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

