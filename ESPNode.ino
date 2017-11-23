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

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
  #define SSD1306_LCDWIDTH                  64
  #define SSD1306_LCDHEIGHT                 48
#include <Adafruit_SSD1306.h>
 
// SCL GPIO5 D1
// SDA GPIO4 D2
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);
 

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS1 2  // DS18B20 1st sensor pin
#define ONE_WIRE_BUS2 14 // DS18B20 2nd sensor pin
OneWire oneWire1(ONE_WIRE_BUS1);
OneWire oneWire2(ONE_WIRE_BUS2);
DallasTemperature ds18b20(&oneWire1);
DallasTemperature ds18b20_2(&oneWire2);

#define ALARM_PIN1 12  // Alarm 1st sensor pin
#define ALARM_PIN2 13  // Alarm 2nd sensor pin
#define BUZZ_PIN 15 // Buzzer pin
#define BUZZ_TONE 1000 // Buzzer tone

#define WIFI_CONNECTING_WAIT 5 // time to wait for connecting to WiFi (sec)
#define WIFI_RECONNECT_DELAY 30 // time to wait before reconnecting to WiFi (~sec)
#define ALARM_INIT_WAIT 60 // time to wait before initialize alarm pin default state (~sec)

const int activityLed = LED_BUILTIN;
const int activityOn = 0;
const int activityOff = 1;

const char* ssid = "VVVHome";
const char* password = "123";

int markPos = 0; // work animation mark position
bool alarmActive = false; // alarm check activity status
bool isAlarm = false; // alarm status
bool buzzerOn = false; // buzzer on/off
int alarmInitWait = ALARM_INIT_WAIT * 1000; // delay before initialize sensors (msec)
int alarmInitState1 = -1, alarmInitState2 = -1; // alarm pins init state
int wifiReconnectDelay = 0; // delay counter before reconnecting to WiFi

ESP8266WebServer server(80);

/*
* Starting the node
*/
void setup(void){
  Serial.begin(115200);
  Serial.println("Begin");


  // Prepare dispay
  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  // init done
  Serial.println("Init done");
 
  // Show image buffer on the display hardware.
  // Since the buffer is intialized with an Adafruit splashscreen
  // internally, this will display the splashscreen.
  display.display();
  delay(500); alarmInitWait -= 500;
 
  // Clear the buffer.
  display.clearDisplay();
 
  // text display tests
  Serial.println("Hello!");
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Hello!");
  display.display();
  delay(500); alarmInitWait -= 500;


  // Prepare sensors
  Serial.println("Sensors:");
  display.println("Sensors:");
  display.display();
  ds18b20.begin();
  ds18b20_2.begin();
  Serial.println("DS18B20 init done.");
  display.println("DS18B20 ok");
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
  delay(500); alarmInitWait -= 500;

  // Connect to WiFi network
  if (!connectToWiFi()) {
    wifiReconnectDelay = WIFI_RECONNECT_DELAY * 2;
    delay(1000); alarmInitWait -= 1000;
  }
  
  // Start the server
  initWebServer();
  initWebUpdate();
  server.begin();
  Serial.println("HTTP server started");
  display.println("HTTP server started");
  display.display();
  
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
  if (alarmInitWait <= 0 && alarmInitState1 < 0) {
    alarmInitState1 = alarm1;
    alarmInitState2 = alarm2;
  }
  
  // alarm check
  if (alarmActive && alarmInitWait <= 0) {
    if (alarm1 != alarmInitState1 || alarm2 != alarmInitState2) {
      isAlarm = true;
    }
  }

  // display
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
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
    display.println(temperature);
    // busser set off
    if (buzzerOn) {
      noTone(BUZZ_PIN);
      buzzerOn = false;
    }
  } else {
    // animation alarm
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

  delay(500);
  if (alarmInitWait > 0) alarmInitWait -= 500;
}



/*
* Try to connect to WiFi
*/
bool connectToWiFi() {
  //display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  Serial.println();
  Serial.println("Scan WiFi networks");
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
      // check
      if (WiFi.SSID(i) == String(ssid))
        ssid_exists = true;
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      delay(10);
    }
    if (!ssid_exists) {
      Serial.println(String(ssid) + " not found.");
      display.println(ssid);
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
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Connected.");
    display.println("IP:");
    display.println(WiFi.localIP());
    display.display();
  
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    return true;
  } else {
    Serial.println("");
    Serial.print("Can not connect to ");
    Serial.println(ssid);
    display.clearDisplay();
    display.setCursor(0,0);
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
* server.on("/inline", [](){
* server.send(200, "text/plain", "this works as well");
* });
*/
void initWebServer() {
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.on("/json", handleJSON);
  server.on("/mac", HTTP_GET, [](){
    serverSendHeaders();
    server.send(200, "text/plain", WiFi.macAddress());
  });
  server.on("/alarm", HTTP_GET, [](){
    String response = String(isAlarm?"1":"0");
    // get arguments
    String set = server.arg("set");
    int wait = atoi(server.arg("wait").c_str());
    // change status
    if (set == "on") {
      alarmActive = true;
      if (wait > 0) {
        
      }
    }
    if (set == "off") {
      alarmActive = false;
      isAlarm = false;
    }
    // response
    serverSendHeaders();
    server.send(200, "text/plain", response);
  });
}

void handleJSON() {
  digitalWrite(activityLed, activityOn);

  String mac = WiFi.macAddress();
  String temperature1 = getTemperature(&ds18b20);
  String temperature2 = getTemperature(&ds18b20_2);
  String s_alarm = (isAlarm)? "true" : "false";
  String s_alarm1 = String(digitalRead(ALARM_PIN1));
  String s_alarm2 = String(digitalRead(ALARM_PIN2));
  String s_alarm_init1 = String(alarmInitState1);
  String s_alarm_init2 = String(alarmInitState2);
  String s_alarm_ready = (alarmInitWait <= 0)? "true" : "false";
  String s_alarm_active = (alarmActive)? "true" : "false";
  
  Serial.println();
  Serial.println("Mac address: " + mac);
  Serial.println("Temperature1: " + temperature1);
  Serial.println("Temperature2: " + temperature2);
  Serial.println("Alarm: " + s_alarm);
  Serial.println("Alarm1: " + s_alarm1);
  Serial.println("Alarm2: " + s_alarm2);
  Serial.println("AlarmDefaultState1: " + s_alarm_init1);
  Serial.println("AlarmDefaultState2: " + s_alarm_init2);
  Serial.println("AlarmReady: " + s_alarm_ready);
  Serial.println("AlarmActive: " + s_alarm_active);
  
  serverSendHeaders();
  String response = "{";
  response += "\"t1\":\"" + temperature1 + "\", ";
  response += "\"t2\":\"" + temperature2 + "\", ";
  response += "\"alarm\":" + s_alarm + ", ";
  response += "\"alarm1\":" + s_alarm1 + ", ";
  response += "\"alarm2\":" + s_alarm2 + ", ";
  response += "\"alarm_init1\":" + s_alarm_init1 + ", ";
  response += "\"alarm_init2\":" + s_alarm_init2 + ", ";
  response += "\"alarm_ready\":" + s_alarm_ready + ", ";
  response += "\"alarm_active\":" + s_alarm_active + ", ";
  response += "\"mac\":\"" + mac + "\"";
  response += "}";
  server.send(200, "application/json", response);
  
  digitalWrite(activityLed, activityOff);
}

void handleRoot() {
  digitalWrite(activityLed, activityOn);
  
  String temperature1 = getTemperature(&ds18b20);
  String temperature2 = getTemperature(&ds18b20_2);
  
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

void serverSendHeaders() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
}

void initWebUpdate(){
  server.on("/web", HTTP_GET, [](){
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });
  server.on("/update", HTTP_POST, [](){
    serverSendHeaders();
    server.send(200, "text/plain", (Update.hasError())?"FAIL":"OK");
    ESP.restart();
  },[](){
    HTTPUpload& upload = server.upload();
    if(upload.status == UPLOAD_FILE_START){
      Serial.setDebugOutput(true);
      //WiFiUDP::stopAll();
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
