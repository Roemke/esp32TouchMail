#include <WiFi.h>
#include <Preferences.h>    
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>    
#include <ESP_Mail_Client.h>
#include "html_files.h"
//define vor dem include -> sonst wird es auf 0 gesetzt nein, muss tatsaechlich in der ElegantOTA.h gesetzt werden 
//#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#include <ElegantOTA.h>
//#include <AsyncElegantOTA.h> //das ein warning, man möge ElegantOTA nutzen ...
//ElegantOTA authentication ueberprueft nur /update, den Rest muesste man selbst machen 
//https://randomnerdtutorials.com/esp32-esp8266-web-server-http-authentication/

//erstmal nach https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/

//#warning AsyncElegantOTA library is deprecated, Please consider moving to newer ElegantOTA library which now 
//comes with an Async Mode. Learn More: https://docs.elegantota.pro/async-mode/

//im folgenden ein Deep-Sleep Beispiel, lasse das mal stehen


 
const String ApNet = "ESP32 (Touch) WebMan";
const String ApIp = "192.168.4.1";
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
int threshold = 40;

RTC_DATA_ATTR unsigned int wakeByTouch = 0;
AsyncWebServer server(80);

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
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
  // <Add your own code here>
}

//Mail funktionen
void smtpCallback(SMTP_Status status){
  Serial.println(status.info());
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount()); //fuer esp32 ginge auch Serial.print direkt
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
  {
    Serial.println("\nNot yet logged in.");
  }
  else
  {
    if (smtp.isAuthenticated())
      Serial.println("\nSuccessfully logged in.");
    else
      Serial.println("\nConnected with no Auth.");
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
  preferences.putString("essid",essid);
  preferences.putString("pass",pass);
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
  else if (var == "UPDATE_LINK")
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
        Serial.print(key);
        Serial.print(" set to: ");
        Serial.println(val);
        //save value
        preferences.putString(key,val);
        return val;
}
void evaluateSetup(AsyncWebServerRequest *request) 
{
  int params = request->params();
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
        threshold = evaluateSingle("treshold",p->value().c_str()).toInt();      
      else if (p->name() == "smtpHost") 
        smtpHost = evaluateSingle("smtpHost",p->value().c_str());
      else if (p->name() == "smtpPort") 
        smtpPort = evaluateSingle("smtpPort",p->value().c_str()).toInt();
  
      ElegantOTA.setAuth(httpdUser.c_str(), httpdPass.c_str());
    }  
  }
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
    deepSleep = 0; //setze auf jeden Fall auf 0
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
    Serial.println("Setting AP (Access Point)");
    WiFi.softAP(ApNet.c_str(), NULL);

    actualIP = WiFi.softAPIP().toString();
    Serial.print("AP IP address: ");
    Serial.println(actualIP); 
    attachServerActions();
    server.begin();

}
//ap aufsetzen, falls keine essid / keine Verbindung möglich
void setupServer()
{
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting up WebServer and Mail for ");
  Serial.println(essid + " / "+ pass + " and Sender/receiver " + sender + " / "+ receiver );
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

  
  Serial.print("IP address of ESP-Device: ");
  Serial.println(actualIP); 

  // Web Server Root URL
}



//----------------------------------------------------


void touchCallback(){
  //placeholder callback function
  Serial.println("touch detected");
}

void setup() {
  Serial.begin(9600);
  Serial.println("Waking up... or first start");
  //lade gespeicherte Daten // setze initiale Werte 
  preferences.begin("settings",false); //"false heißt read/write"
  essid = preferences.getString("essid", "");  
  pass = preferences.getString("pass", "");
  httpdUser = preferences.getString("httpdUser", "admin");
  httpdPass = preferences.getString("httpdPass", "admin");
  sender = preferences.getString("sender", "");
  receiver = preferences.getString("receiver", "");
  receiverName = preferences.getString("receiverName", "");
  appPass = preferences.getString("appPass", "");
  smtpPort = preferences.getString("smtpPort", "587").toInt();
  smtpHost = preferences.getString("smtpHost",smtpHost);
  deepSleep = preferences.getString("deepSleep", "0").toInt();
  touchPin = preferences.getString("touchPin", "4").toInt();
  threshold = preferences.getString("threshold", "40").toInt();

  //------------------------ota------------------
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
  
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TOUCHPAD)
  {
    strcpy(actualMailSubject,"Klappe: Touch");
    strcpy(actualMailBody,"Der Touchevent auf dem Knopf an der Klappe hat den ESP geweckt, daher Post. ");
  }
  touchAttachInterrupt(touchPin, touchCallback, threshold);
    // Set Authentication Credentials
  ElegantOTA.setAuth(httpdUser.c_str(), httpdPass.c_str());
  previousMillis = (unsigned long) millis();
  esp_sleep_enable_touchpad_wakeup(); 
}
void loop() {
  
  //und check, ob wir eine Mail zu versenden haben, hier wg. der Probleme mit dem RTOS WatchDog
  //hier ginge es sicher auch aus dem setup, aber testMail Button loest ueber web aus
  if (strlen(actualMailSubject) != 0 )  
  {
    String msg = "Sende Mail mit Subject ";
    msg += actualMailSubject;
    Serial.println(msg.c_str());
    sendMailMessage(actualMailSubject,actualMailBody);
    actualMailSubject[0] = actualMailBody[0] = 0;
  }
  else
  {
    unsigned long currentMillis = millis();
    if (deepSleep && !strlen(actualMailSubject) && currentMillis > previousMillis + deepSleep*1000)
    {
      Serial.println("Going to sleep now");
      esp_deep_sleep_start();
      Serial.println("This will never be printed");
    }
  }
  ElegantOTA.loop();
}
