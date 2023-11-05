#include <WiFi.h>
#include <Preferences.h>    
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>    
#include <ESP_Mail_Client.h>
#include "html_files.h"
#include <FastLED.h>
//define vor dem include -> sonst wird es auf 0 gesetzt nein, muss tatsaechlich in der ElegantOTA.h gesetzt werden, da ist visual studio besser, da 
//per Projekt einstellungen gespeichert werden können 
//#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#include <ElegantOTA.h>
#include "ownLists.h"
//#include <AsyncElegantOTA.h> //das ein warning, man möge ElegantOTA nutzen ...
//ElegantOTA authentication ueberprueft nur /update, den Rest muesste man selbst machen 
//https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/

//erstmal nach https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/

//#warning AsyncElegantOTA library is deprecated, Please consider moving to newer ElegantOTA library which now 
//comes with an Async Mode. Learn More: https://docs.elegantota.pro/async-mode/

//im folgenden ein Deep-Sleep Beispiel, lasse das mal stehen


 
const String ApNet = "ESP32 (Touch) WebMan";
const String ApIp = "192.168.4.1";
bool inApMode = false;
String sendMails = "checked";
ObjectList <String> startMeldungen(32); //dient zum Puffern der Meldungen am Anfang


String actualIP = ApIp;
String essid = "";
String pass = "";
String httpdUser = "admin";
String httpdPass = "admin";
//email
String sender= "";
String receiver = "";
String receiverName = "";
String appPass = "";
//mail funktion -----------------------------------------------------------------------
String smtpHost  = "smtp.gmail.com";
int smtpPort =  esp_mail_smtp_port_587 ; //constante
bool mailSendTriggered = false;
bool inOtaUpdate = false;
SMTPSession smtp;
Session_Config config;

const int shortStringS = 64;
const int longStringS = 256; 
char actualMailSubject[shortStringS]="";
char actualMailBody[longStringS]="";
Preferences preferences;           

// Timer variables
unsigned long previousMillis = 0;
const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)
unsigned long deepSleep = 0; //hier in Sekunden, 0: deepSleep ist aus 
int touchPin = 4; // GPIO 4 is touch 0 
int threshold = 52; //40;

RTC_DATA_ATTR unsigned int wakeByTouch = 0;
AsyncWebServer server(80);
//fuer den Websocket
AsyncWebSocket ws("/ws");

unsigned long ota_progress_millis = 0;


//----------------------------interne WLED
#define NUM_LEDS 1     //Number of RGB LED beads
#define DATA_PIN D8    //The pin for controlling RGB LED
#define LED_TYPE NEOPIXEL    //RGB LED strip type
CRGB leds[NUM_LEDS];    //Instantiate RGB LED
//---------------------------------------------
void wsMessage(const char *message,AsyncWebSocketClient * client = 0)
{ 
  const char * begin = "{\"action\":\"message\",\"text\":\"";
  const char * end = "\"}";
  char * str = new char[strlen(message)+strlen(begin)+strlen(end)+24];//Puffer +24 statt +1 :-)
  strcpy(str,begin);
  strcat(str,message);
  strcat(str,end);
  if (!client)
    ws.textAll(str);
  else
    client->text(str);
    
  delete [] str;
}

void wsMsgSerial(const char *message, AsyncWebSocketClient * client = 0)
{
  wsMessage(message,client); 
  Serial.print(message);
  int len = strlen(message); //ab<br>
  if (!strcmp(message + len - 4,"<br>"))
    Serial.print("\n");
}
void wsMsgSerialStart(const char *message)
{
  Serial.println(message);
  startMeldungen.add(message);
}


void onWSEvent(AsyncWebSocket     *server,  //
             AsyncWebSocketClient *client,  //
             AwsEventType          type,    // the signature of this function is defined
             void                 *arg,     // by the `AwsEventHandler` interface
             uint8_t              *data,    //
             size_t                len)    
{
    // we are going to add here the handling of
    // the different events defined by the protocol
    String msg; 
     switch (type) 
     {
        case WS_EVT_CONNECT:
            //bei einem connect werden die Startmeldungen ausgegeben, hier muesste ich doch schon die Daten senden können?  
            msg = String("WebSocket client ");// + String(client->id()) + String(" connected from ") +  client->remoteIP().toString();
            wsMsgSerial(msg.c_str());
            msg = String("Anzahl Clients: ") + server->count()+String("<br>");
            wsMsgSerial(msg.c_str());     
            wsMessage(startMeldungen.htmlLines().c_str(),client);
            startMeldungen.clear();
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            //handleWebSocketMessage(arg, data, len); arbeite hier nicht mit sockets sondern mit ajax-Requests
            //moechte den socket nur um Daten auch senden zu koennen
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
     }
}
void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
  inOtaUpdate = true;
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  inOtaUpdate = false;
  // <Add your own code here>
}

//Mail funktionen
void smtpCallback(SMTP_Status status){
  char msg[64];
  Serial.println(status.info());
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount()); //fuer esp32 ginge auch Serial.print direkt
    sprintf(msg,"Message sent success: %d<br>", status.completedCount());
    startMeldungen.add(msg);
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients);
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
    smtp.sendingResult.clear();
  }
}
void setupMail() 
{
   /*  Set the network reconnection option */
  MailClient.networkReconnect(true); //MailClient wird in der .h extern definiert und findet sich in der .cpp?
    smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);
  /* Set the session config */
  config.server.host_name = smtpHost;
  config.server.port = smtpPort;
  config.login.email = sender;
  config.login.password = appPass;
  config.login.user_domain = smtpHost;

  /* Set the NTP config time */
  config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
  config.time.gmt_offset = 1;
  config.time.day_light_offset = 1;
  
}

void sendMailMessage(char * subj, char * text)
{
  //Aufbau der E-Mail
  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = F("ESP32 Touch");
  message.sender.email = sender;
  message.subject = subj;
  message.addRecipient(receiverName, receiver);

  String textMsg = text;
  message.text.content = textMsg;
  message.text.charSet = F("utf-8");
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  String headerA = "Message-ID: <";
  headerA += sender;
  headerA += ">";
  message.addHeader(headerA.c_str());

  if (!smtp.connect(&config))
  {
    ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
    return;
  }
  if (!smtp.isLoggedIn())
    wsMsgSerialStart("Not yet logged in");
  else
  {
    if (smtp.isAuthenticated())
      wsMsgSerialStart("Successfully logged in.");
    else
      wsMsgSerialStart("<br>Connected with no Auth.");
  }

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());

  // to clear sending result log
  smtp.sendingResult.clear();
}


//-----------------------------------
void resetWifiSettings()
{
  essid = "";
  pass = "";
  preferences.begin("settings",false); //"false heißt read/write"  
  preferences.putString("essid",essid);
  preferences.putString("pass",pass);
  preferences.end();
  wsMsgSerial("<br>Wifi Settings reset<br>");
  ESP.restart();
}
//zum Server-Kram
//filter
String processor(const String& var)
{
  String result = "";
  if (var == "ESSID")
    result = essid;
  else if (var == "PASS")
    result = pass;
  else if (var == "HTTPDUSER")
    result = httpdUser;
  else if (var == "HTTPDPASS")
    result = httpdPass;
  else if (var == "UPDATELINK")
    result = "<a href=\"http://" + actualIP +"/update\"> Update </a>";
  else if (var == "MAC")
    result = WiFi.macAddress();
  else if (var ==  "RESETURL")
    result = "http://" + actualIP +"/resetWifi";
  else if (var ==  "TESTMAILURL")
    result = "http://" + actualIP +"/testMail";
  else if (var ==  "RESTARTURL")
    result = "http://" + actualIP +"/restart";
  else if (var ==  "APIP")
    result = ApIp;
  else if (var ==  "APNET")
    result = ApNet;
  else if (var ==  "RECEIVER")
    result = receiver;
  else if (var ==  "RECEIVERNAME")
    result = receiverName;
  else if (var ==  "SENDMAILS")
    result = sendMails;
  else if (var ==  "SENDER")
    result = sender;
  else if (var ==  "APPPASS")
    result = appPass;
  else if (var ==  "SMTPPORT")
    result = smtpPort;
  else if (var ==  "SMTPHOST")
    result = smtpHost;
  else if (var ==  "DEEPSLEEP")
    result = deepSleep;
  else if (var ==  "TOUCHPIN")
    result = touchPin;
  else if (var ==  "THRESHOLD")
    result = threshold;
  return result;
}

void testMail(AsyncWebServerRequest *request)
{
  char answer[] = "{\"testMail\":\"accepted\"}"; 
  request->send_P(200, "application/json",answer); //scheine hier eine Antwort geben zu müssen, da sonst fetch häufiger abfragt
  
  Serial.println("Bin in Sende Testmail");
  strcpy(actualMailSubject,"Klappe: Touch Test");
  strcpy(actualMailBody,"Der Touchevent auf dem Knopf an der Klappe hat den ESP geweckt, daher Post - hier per Button also ein Test.  ");
}

void resetWifi(AsyncWebServerRequest *request)
{
    char answer[] = "{\"resetWifi\":\"done\"}"; 
    Serial.println("Reset WifiSettings by Client requested - do it, reboot, sending to client: ");
    Serial.println(answer);

    request->send_P(200, "application/json",answer);
    delay(1000);
    resetWifiSettings(); 
}

void restart(AsyncWebServerRequest *request)
{
    char answer[] = "{\"restart\":\"done\"}"; 
    Serial.println("Restart button,  restart done, sending to client: ");
    Serial.println(answer);

    request->send_P(200, "application/json",answer);
    delay(1000);
    ESP.restart();
}


String evaluateSingle(const char * key, const char *val)
{
        String msg = String(key) +" set to: " + val ;
        wsMsgSerialStart(msg.c_str());
        //save value
        preferences.putString(key,val);
        return val;
}
void evaluateSetup(AsyncWebServerRequest *request) 
{
  int params = request->params();
      /*
        vorsicht checkboxen rauben den letzten Nerv, nur wenn checked wird überhaupt etwas übertragen, nämlich der 
        value. Ist der nicht gesetzt wird auch nichts übertragen - sicher, eigentlich sollte on übertragen werden
      */
  preferences.begin("settings",false); //"false heißt read/write"  
  sendMails=evaluateSingle("sendMails","notChecked");
  for(int i=0;i<params;i++)
  {
    AsyncWebParameter* p = request->getParam(i);
    if(p->isPost())
    {
      // HTTP POST ssid value
      
      if (p->name() == "essid") 
        essid = evaluateSingle("essid",p->value().c_str());
      else if (p->name() == "pass") 
        pass = evaluateSingle("pass",p->value().c_str());      
      else if (p->name() == "httpdUser") 
        httpdUser = evaluateSingle("httpdUser",p->value().c_str());      
      else if (p->name() == "httpdPass") 
        httpdPass = evaluateSingle("httpdPass",p->value().c_str());      
      else if (p->name() == "sendMails")
        sendMails = evaluateSingle("sendMails","checked");//wenn was kommt, dann checked
      else if (p->name() == "sender") 
        sender = evaluateSingle("sender",p->value().c_str());      
      else if (p->name() == "appPass") 
        appPass = evaluateSingle("appPass",p->value().c_str());      
      else if (p->name() == "receiver") 
        receiver = evaluateSingle("receiver",p->value().c_str());      
      else if (p->name() == "receiverName") 
        receiverName = evaluateSingle("receiverName",p->value().c_str());
      else if (p->name() == "deepSleep") 
        deepSleep = evaluateSingle("deepSleep",p->value().c_str()).toInt();      
      else if (p->name() == "touchPin") 
        touchPin = evaluateSingle("touchPin",p->value().c_str()).toInt();      
      else if (p->name() == "threshold") 
        threshold = evaluateSingle("threshold",p->value().c_str()).toInt();      
      else if (p->name() == "smtpHost") 
        smtpHost = evaluateSingle("smtpHost",p->value().c_str());
      else if (p->name() == "smtpPort") 
        smtpPort = evaluateSingle("smtpPort",p->value().c_str()).toInt();
      
      ElegantOTA.setAuth(httpdUser.c_str(), httpdPass.c_str());
    }  
  }

  preferences.end();
  if(!request->authenticate(httpdUser.c_str(), httpdPass.c_str()))
    return request->requestAuthentication();
  request->send_P(200, "text/html", setup_html, processor); //durch processor filtern     

  //request->send(200, "text/plain", "Done. ESP will restart, connect to your router and search for ip of this device :-)");
  //delay(3000);
  //ESP.restart();
}

void attachServerActions()
{
  //haenge standardkram an, teils als anonyme Funk, teils mit Namen
  server.on("/resetWifi", HTTP_GET, resetWifi);
  server.on("/restart", HTTP_GET, restart);
  server.on("/testMail", HTTP_GET, testMail);
  server.on("/", HTTP_GET,[](AsyncWebServerRequest *request)
  {
    if(!request->authenticate(httpdUser.c_str(), httpdPass.c_str()))
      return request->requestAuthentication();

    request->send_P(200, "text/html", setup_html, processor); //durch processor filtern     
  });
  server.on("/", HTTP_POST, evaluateSetup);
  server.onNotFound( [](AsyncWebServerRequest *request) 
  {
    request->send(404, "text/plain", "Not found - ich kann das nicht");
  });
  server.on("/favicon.ico", [](AsyncWebServerRequest *request)
  {
    request->send(204);//no content
  });
}
//ap aufsetzen, falls keine essid / keine Verbindung möglich
void setupAP()
{
    // Connect to Wi-Fi network with SSID and password
    wsMsgSerialStart("Setting AP (Access Point)");
    WiFi.softAP(ApNet.c_str(), NULL);

    actualIP = WiFi.softAPIP().toString();
    String msg = "AP IP address: " + actualIP;
    wsMsgSerialStart(msg.c_str());
    attachServerActions();
    server.begin();
    deepSleep = 0;
    inApMode = true;

}
//
void setupServer()
{
  // Connect to Wi-Fi network with SSID and password

  String msg = "Setting up WebServer and Mail for " + essid + " / "+ pass + " and Sender/receiver " + sender + " / "+ receiver ;
  wsMsgSerialStart(msg.c_str());
  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  WiFi.begin(essid, pass);
  while(WiFi.status() != WL_CONNECTED) 
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) 
    {
      Serial.println("Failed to connect, reset Wifi-Settings and reboot.");
      resetWifiSettings();
    }
  }

  actualIP = WiFi.localIP().toString();
  attachServerActions();
  server.begin();
  inApMode = false;
  
  Serial.print("IP address of ESP-Device: ");
  Serial.println(actualIP); 

  // Web Server Root URL
}



//----------------------------------------------------

void print_wakeup_reason(esp_sleep_wakeup_cause_t wakeup_reason)
{          

  switch(wakeup_reason) {             // Check the wake-up reason and print the corresponding message
    case ESP_SLEEP_WAKEUP_EXT0 : wsMsgSerialStart("Wakeup caused by external signal using RTC_IO<br>"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : wsMsgSerialStart("Wakeup caused by external signal using RTC_CNTL<br>"); break;
    case ESP_SLEEP_WAKEUP_TIMER : wsMsgSerialStart("Wakeup caused by timer<br>"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : wsMsgSerialStart("Wakeup caused by touchpad<br>"); break;
    case ESP_SLEEP_WAKEUP_ULP : wsMsgSerialStart("Wakeup caused by ULP program<br>"); break;
    default : wsMsgSerialStart("Wakeup was not caused by deep sleep:<br>"); break; //wakeUpreason ist dann i.d.R. 0
  }
}

void touchCallback(){
  //placeholder callback function
  Serial.print(" t");//hier fuehrt ein wsMsgSerial zum "hängen"
}
void readPreferences()
{
  preferences.begin("settings",false); //"false heißt read/write"
  essid = preferences.getString("essid", "");  
  pass = preferences.getString("pass", "");
  httpdUser = preferences.getString("httpdUser", "admin");
  httpdPass = preferences.getString("httpdPass", "admin");
  sendMails = preferences.getString("sendMails","checked");
  sender = preferences.getString("sender", "");
  receiver = preferences.getString("receiver", "");
  receiverName = preferences.getString("receiverName", "");
  appPass = preferences.getString("appPass", "");
  smtpPort = preferences.getString("smtpPort", "587").toInt();
  smtpHost = preferences.getString("smtpHost",smtpHost);
  deepSleep = preferences.getString("deepSleep", "0").toInt();
  touchPin = preferences.getString("touchPin", "4").toInt();
  threshold = preferences.getString("threshold", "52").toInt();
  preferences.end();
}

void setup() {
  Serial.begin(115200);
  wsMsgSerialStart("Waking up... or first start");
  //led ein 
  FastLED.addLeds<LED_TYPE, DATA_PIN>(leds, NUM_LEDS);     //Initialize RGB LED
  leds[0] = CRGB::Blue;     // LED shows blue light
  FastLED.show();
  //lade gespeicherte Daten // setze initiale Werte 
  readPreferences();

  ws.onEvent(onWSEvent);
  server.addHandler(&ws); //WebSocket dazu 


  //------------------------ota------------------
  wsMsgSerialStart("Setup OTA<br>");
  ElegantOTA.begin(&server);    // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);
  //---------------------------------------------
  
  if (essid == "")
    setupAP();
  else 
    setupServer(); //kurze zeit wifi, Mail

  setupMail();
  
    // Set Authentication Credentials
  
  ElegantOTA.setAuth(httpdUser.c_str(), httpdPass.c_str());
  previousMillis = (unsigned long) millis();
  //esp_sleep_enable_touchpad_wakeup();
  
  //#define uS_TO_S_FACTOR 1000000ULL   // Conversion factor from microseconds to seconds
  //#define TIME_TO_SLEEP  5  
    //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR); 
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  print_wakeup_reason(wakeup_reason);   // Print the wake-up reason 
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TOUCHPAD )
  {
    if ( sendMails == "checked")
    {
      strcpy(actualMailSubject,"Klappe: Touch");
      strcpy(actualMailBody,"Der Touchevent auf dem Knopf an der Klappe hat den ESP geweckt, daher Post. ");
      wsMsgSerialStart("Prepare sending of emails</n>");
    }
    else
      wsMsgSerialStart("Sendmails is disabled by webinterface<br>");
  }
  char threshString[128];
  sprintf(threshString, "Set Wake up Thresh to %d pin is %d <br>",threshold,touchPin) ;
  wsMsgSerialStart(threshString);
  touchAttachInterrupt(touchPin, touchCallback, threshold);
  esp_sleep_enable_touchpad_wakeup();   
}
void loop() {
  
   // ws.cleanupClients(); //wurde empfohlen einmal pro sekunde, scheint aber den OTA-Prozess zu stören 
    unsigned long currentMillis = millis();

  //und check, ob wir eine Mail zu versenden haben, hier wg. der Probleme mit dem RTOS WatchDog
  //hier ginge es sicher auch aus dem setup, aber testMail Button loest ueber web aus
  if (strlen(actualMailSubject) != 0 )  
  {
    String msg = "Sende Mail mit Subject ";
    msg += actualMailSubject;
    wsMsgSerial(msg.c_str());
    sendMailMessage(actualMailSubject,actualMailBody);
    actualMailSubject[0] = actualMailBody[0] = 0;
  }
  else
  {
    //zum kalibrieren, können wir aber drin lassen
    int touchValue = touchRead(touchPin);
    char touchString[16];
    if ((currentMillis - previousMillis ) % 500 == 0 )
    {
      sprintf(touchString,"%d ", touchValue);
      wsMsgSerial(touchString);
    }
    //----------------------
    if (deepSleep && !inApMode && !inOtaUpdate && !strlen(actualMailSubject) && currentMillis > previousMillis + deepSleep*1000)
    {
      
      wsMsgSerial("<br>Going to sleep in 5 seconds<br>");
      char * closeMsg = "{\"action\":\"close\"}";
      ws.textAll(closeMsg);

      delay(5000);
      ws.closeAll();
      FastLED.clear(true);
      esp_deep_sleep_start();
      Serial.println("This will never be printed");
    }
  }
  //uint32_t touchTVal;
  //touch_pad_sleep_get_threshold(touchPin,&touchTVal);
  //String msg = "Thresh is " + touchTVal + "<br>";
  //wsMsgSerial(msg.c_str());
  ElegantOTA.loop();
  
}
