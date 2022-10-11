#include <Arduino.h>
#include <FS.h> 
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

//#define BLYNK_DEBUG        // Optional, this enables more detailed prints
//#define BLYNK_PRINT Serial1 // Defines the object that is used for printing

#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>

#include "ESPTelnet.h"

static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 2;     // Central European Time

char blynk_server[40] = "xitos.uk.to";
char api_token[40] = "YOUR_API_TOKEN";
//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
//  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void launch_captive_portal(){
  Serial.println("captive portal");
 if (SPIFFS.begin()) {
//    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
//      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
//        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        //serializeJson(json, Serial);
        if ( ! deserializeError ) {
          
//          Serial.println("\nparsed json");
          strcpy(blynk_server, json["blynk_server"]);
          strcpy(api_token, json["api_token"]);
        } else {
//          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
//    Serial.println("failed to mount FS");
  }

  WiFiManagerParameter custom_blynk_server("server", "Blynk server", blynk_server, 40);
  WiFiManagerParameter custom_api_token("apikey", "API token", api_token, 40);
  
  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.addParameter(&custom_blynk_server);
  wm.addParameter(&custom_api_token);

  if(WiFi.status() == WL_CONNECTED){
    wm.startConfigPortal("WiFi_Module","eimaimparmpas327");
  }else{
  
  bool res;
  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
  res = wm.autoConnect("WiFi_Module","eimaimparmpas327"); // password protected ap
  }
  //read updated parameters
  strcpy(blynk_server, custom_blynk_server.getValue());
  strcpy(api_token, custom_api_token.getValue());

   //save the custom parameters to FS
  if (shouldSaveConfig) {
//    Serial.println("saving config");
#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    json["blynk_server"] = blynk_server;
    json["api_token"] = api_token;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
//      Serial.println("failed to open config file for writing");
    }

    //serializeJson(json, Serial);
    serializeJson(json, configFile);

    configFile.close();
    //end save
  }
}

#include "inverter.h"
extern QpigsMessage _qpigsMessage;
extern QmodMessage _qmodMessage;
extern QpiriMessage _qpiriMessage;
extern QpiwsMessage _qpiwsMessage;
extern QpiMessage _qpiMessage;

extern QRaw _qRaw;
WidgetRTC rtc;
WidgetLCD lcd(V0);
WidgetTerminal term(V51);
WidgetTable table(V32);

BLYNK_CONNECTED(){
  delay(200);
  rtc.begin();
  //term.clear();
  Blynk.virtualWrite(V51, "\n");
  Blynk.virtualWrite(V51, ESP.getResetReason() + '\r');
}
BLYNK_WRITE(V30){
  int itm = param.asInt();
  if(sendCommand("PCP0"+String(itm - 1))){
    Blynk.notify("setting send");
  }else{
    Blynk.notify("crc error");
  }
  
}
BLYNK_WRITE(V31){
  int itm = param.asInt();
 if(sendCommand("POP0"+String(itm - 1))){
  Blynk.notify("setting send");
  }else{
    Blynk.notify("crc error");
  }
}
BLYNK_WRITE(V52){
  Blynk.notify("Launching Captive Portal");
  launch_captive_portal();
}

enum serialsource {blnk, tlnet};
ESPTelnet telnet;
serialsource src;
#define tl_timeout 1000
uint32_t tl_tm;
// bool req_captive;

// IRAM_ATTR void _request_captive(){
//   req_captive = 1;  
//   Serial.println("req portal");
// }

void setup() {
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  
  launch_captive_portal();
  WiFi.setHostname("WiFi_Module");
  ArduinoOTA.setHostname("WiFi_Module");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.onStart([](){
    Blynk.virtualWrite(V51, "\n...UPDATING...\n");
  });
  ArduinoOTA.begin();
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(16, INPUT_PULLUP);
  if(digitalRead(16) == 0){
    Serial.println("req portal");
    //req_captive = 0;
    launch_captive_portal();
  }
  // attachInterrupt(digitalPinToInterrupt(16), _request_captive, FALLING);

  //Serial1.begin(115200); 
  delay(3000); 
  Blynk.config(api_token, blynk_server, 8080);
  //Blynk.config(api_token, IPAddress(192,168,2,2), 8080);

  telnet.setLineMode(true);
  telnet.onInputReceived([](String str) {
    tl_tm = millis();
    src = tlnet;
    //String temp = stringcomm(str);
    str += '\r';
    Serial.print(str);
    Blynk.virtualWrite(V51, "Client: " + str + '\n');
  });
  telnet.onConnect([](String ip){
    tl_tm = millis();
    src = tlnet;
    Serial.println("Client Connected " + ip);
    Blynk.virtualWrite(V51, "Client Connected " + ip + '\n');
  });
  telnet.onDisconnect([](String ip){
    tl_tm = millis();
    src = blnk;
    Serial.println("Client Disconnected " + ip);
    Blynk.virtualWrite(V51, "Client Disconnected " + ip + '\n');
  });
  bool res = telnet.begin(23);

  while (!Blynk.connected()){
    ArduinoOTA.handle();
    Blynk.run();
    telnet.loop();
    if(Serial.available()){
      String _input = Serial.readStringUntil('\n');
      telnet.println(_input);
      Serial.flush();
    }
  }

  if(res){
    Serial.println("telnet started");
    Blynk.virtualWrite(V51, "telnet started\n");
  }else{
    Serial.println("tellnet failled");
    Blynk.virtualWrite(V51, "tellnet failled\n");
  }
  
  setSyncInterval(10 * 60); // Sync interval in seconds (10 minutes)

  Serial.begin(2400);

}

#define wp_refresh 1000
#define tl_refresh 100
uint32_t tm_wp, tm_kw, tm_tl;
float hour_kw[4], day_kw[4], month_kw[4];
bool askInverterOnce = 1;
uint16_t blk_num, req_id;

#define htosval 3600.0
int current_hour, current_day, current_month;
void calculatekw(){
  uint32_t tm_passed = (millis() - tm_kw) / 1000;
  tm_kw = millis();
  hour_kw[0] += (_qpigsMessage.solarW / htosval) * tm_passed;
  hour_kw[1] += (((_qpigsMessage.battDischargeA - _qpigsMessage.battChargeA)* _qpigsMessage.battV) / htosval) * tm_passed;
  hour_kw[2] += (_qpigsMessage.acOutW / htosval) * tm_passed;
  hour_kw[3] += ((_qpigsMessage.battChargeA - (_qpigsMessage.solarW - _qpigsMessage.acOutW) + (_qpigsMessage.acOutW - _qpigsMessage.battDischargeA) ) / htosval) * tm_passed;
  
  if(hour() != current_hour){
    current_hour = hour();
    day_kw[0] += hour_kw[0];
    day_kw[1] += hour_kw[1];
    day_kw[2] += hour_kw[2];
    day_kw[3] += hour_kw[3];
    hour_kw[0] = 0;
    hour_kw[1] = 0;
    hour_kw[2] = 0;
    hour_kw[3] = 0;
    Blynk.virtualWrite(V20, day_kw[0]);
    Blynk.virtualWrite(V21, day_kw[1]);
    Blynk.virtualWrite(V22, day_kw[2]);
    Blynk.virtualWrite(V23, day_kw[3]);
  }

  if(day() != current_day){
    current_day = day();
    month_kw[0] += day_kw[0];
    month_kw[1] += day_kw[1];
    month_kw[2] += day_kw[2];
    month_kw[3] += day_kw[3];
    day_kw[0] = 0;
    day_kw[1] = 0;
    day_kw[2] = 0;
    day_kw[3] = 0;
  }
  if(month() != current_month){
    current_month = month();
    Blynk.virtualWrite(V40, month_kw[0]);
    Blynk.virtualWrite(V41, month_kw[1]);
    Blynk.virtualWrite(V42, month_kw[2]);
    Blynk.virtualWrite(V43, month_kw[3]);
    month_kw[0] = 0;
    month_kw[1] = 0;
    month_kw[2] = 0;
    month_kw[3] = 0;
    
  }
  
}

void loop() {

  if(WiFi.status() == WL_CONNECTED){
    ArduinoOTA.handle();
    Blynk.run();
    telnet.loop();
  }

  if(millis() - tl_tm > tl_timeout && src == tlnet){
    src = blnk;
  }
  if(millis() - tm_tl > tl_refresh && src == tlnet){
    tm_tl = millis();
    if(Serial.available()){
      String _input = Serial.readStringUntil('\r');
      telnet.println(_input);
      Blynk.virtualWrite(V51, "dev: " + _input + "\n");
      Serial.flush();
    }
  }

  if(millis() - tm_wp > wp_refresh && src == blnk){
    tm_wp = millis();
    if (askInverterOnce) //ask the inverter once to get static data
    {
      requestInverter(QPI); //just send for clear out the serial puffer and resolve the first NAK
      requestInverter(QPI);
      requestInverter(QPIRI);
      requestInverter(QMCHGCR);
      requestInverter(QMUCHGCR);
      askInverterOnce = false;
    }

    switch (req_id)
    {
    case 0:
      requestInverter(QMOD);
      req_id++;
      break;
    case 1:
      requestInverter(QPIGS);
      req_id++;
      break;
    case 2:
      requestInverter(QPIWS);
      calculatekw();
      req_id++;
      break;
    default:
      req_id = 0;
      
      // term.println("hello");
      //Blynk.virtualWrite(V51, "hellow");
      break;
    }
    
    

    switch(Blynk.connected()? blk_num : 99){
      case 0:
        Blynk.virtualWrite(V1, _qpigsMessage.solarV * _qpigsMessage.solarA);
        Blynk.virtualWrite(V2, _qpigsMessage.acOutPercent);
        Blynk.virtualWrite(V3, _qpigsMessage.solarV);
        Blynk.virtualWrite(V4, _qpigsMessage.solarA);
        Blynk.virtualWrite(V5, _qpigsMessage.acOutV);
        blk_num++;
        break;
      case 1:
        Blynk.virtualWrite(V6, _qpigsMessage.acOutW / _qpigsMessage.acOutV);
        Blynk.virtualWrite(V7, _qpigsMessage.battPercent);
        Blynk.virtualWrite(V8, _qpigsMessage.battV);
        if(_qpigsMessage.battDischargeA - _qpigsMessage.battChargeA > 0){
          Blynk.setProperty(V10, "color", "#ED9D00");//yelloow
          Blynk.virtualWrite(V10, (_qpigsMessage.battDischargeA - _qpigsMessage.battChargeA));
          Blynk.virtualWrite(V9, (_qpigsMessage.battDischargeA - _qpigsMessage.battChargeA) * _qpigsMessage.battV);
        }else{
          Blynk.setProperty(V10, "color", "#23C48E");//green
          Blynk.virtualWrite(V10, (_qpigsMessage.battChargeA - _qpigsMessage.battDischargeA));
          Blynk.virtualWrite(V9, (_qpigsMessage.battChargeA - _qpigsMessage.battDischargeA) * _qpigsMessage.battV);
        }
        blk_num++;
        break;
      case 2:
        Blynk.virtualWrite(V11, _qpigsMessage.gridV);
        Blynk.virtualWrite(V12, _qpigsMessage.ischarging?255:0);
        Blynk.virtualWrite(V13, _qpigsMessage.acOutV?255:0);
        Blynk.virtualWrite(V50, _qpigsMessage.heatSinkDegC);
        Blynk.virtualWrite(V14, _qpiwsMessage.inverterFault?255:0);
        blk_num++;
        break;
      case 3:
         Blynk.virtualWrite(V15, _qpigsMessage.acOutW);
         Blynk.virtualWrite(V24, (_qpigsMessage.battChargeA * _qpigsMessage.chargingfromAC) * _qpigsMessage.gridV);//grid kw
         Blynk.virtualWrite(V20, _qpigsMessage.sccBattV);
         Blynk.virtualWrite(V21, _qpigsMessage.busV);
        // Blynk.virtualWrite(V18, _qpigsMessage.battV);
        lcd.print(0,0, "Mode:");
        lcd.print(0,1, _qmodMessage.operationMode);
        blk_num++;
        break;
      // case 4:
      //   //Blynk.virtualWrite(V20, _qpigsMessage.battPercent);//solar kw
      //   //Blynk.virtualWrite(V21, _qpigsMessage.battDischargeA - _qpigsMessage.battChargeA);//battery kw
      //   //Blynk.virtualWrite(V22, 0);//load kw
      //   //Blynk.virtualWrite(V23, _qpigsMessage.battV);//grid kw
      //   lcd.print(0,0, "Mode:");
      //   lcd.print(0,1, _qmodMessage.operationMode);
      //   blk_num++;
      //   break;
      default:
      blk_num = 0;
      break;
    }

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  

  if(digitalRead(16) == 0){
    //Serial.println("req portal");
    Blynk.virtualWrite(V51, "req portal\n");
    //req_captive = 0;
    launch_captive_portal();
  }
}