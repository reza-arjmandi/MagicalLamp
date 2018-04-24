#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <Thread.h>

Thread thread = Thread();
ESP8266WiFiMulti WiFiMulti;

const int LdrPin = A0;
const int ButtonPin = 4;
const int RedPin = 15;
const int GreenPin = 12;
const int BluePin = 13;
const int DelayMs = 50;
const int IntensityThreshold = 70;

const char* ssid = "AndroidAP2";
const char* password = "cfjr806212";
const char* host = "Witty";

bool MagicalMode = true;

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {
  String text = String((char *) &payload[0]);
  char * textC = (char *) &payload[0];
  String rssi;
  String LDRvalue;
  String ButtonState;
  String temp;
  int nr;
  int on;
  uint32_t rmask;
  int i;
  char b[10];   //declaring character array
  String str;  //declaring string

  switch(type) {
      //case WStype_DISCONNECTED:
      //    break;
      case WStype_CONNECTED:
          {
              IPAddress ip = webSocket.remoteIP(num);
      
              // send message to client
              delay(5);
              webSocket.sendTXT(num, "C");
          }
          break;
      case WStype_TEXT:

          // send data to all connected clients
          // webSocket.broadcastTXT("message here");
        
          switch(payload[0]){

            case '#':  // RGB LED
              {
              // we get RGB data
              uint32_t rgb = (uint32_t) strtol((const char *) &payload[1], NULL, 16);
              // decode and scale it to max on PWM i.e. 1024
              analogWrite(RedPin, ((rgb >> 16) & 0xFF)*4);
              analogWrite(GreenPin, ((rgb >> 8) & 0xFF)*4);
              analogWrite(BluePin, ((rgb >> 0) & 0xFF)*4);
              delay(5);
              webSocket.sendTXT(0,"OK");
              }
              break;

            case 'p': // ping, will reply pong
              delay(5);
              webSocket.sendTXT(0,"pong");
              break;

            case 'e': case 'E':   //Echo
              delay(5);
              webSocket.sendTXT(0,text);
              break;

            case 'M':
              if(payload[1] == 'D')
              {
                MagicalMode = false;
              }
              else
              {
                MagicalMode = true;
              }
              delay(5);
              webSocket.sendTXT(0,"OK");  
              break;

            default:
              delay(5);
              webSocket.sendTXT(0,"**** UNDEFINED ****");
              break;
          }
          break;
      
      case WStype_BIN:
          hexdump(payload, lenght);
          // send message to client
          // webSocket.sendBIN(num, payload, lenght);
          break;
}
}

//holds the current upload
File fsUploadFile;

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = server.arg("dir");
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  server.send(200, "text/json", output);
}

void WriteToRgbLed(unsigned int R, unsigned int G, unsigned int B)
{
  analogWrite(RedPin, R);
  analogWrite(GreenPin, G);
  analogWrite(BluePin, B);
}

int GetLightIntensity()
{
  return analogRead(LdrPin);
}

void IncreaseBlue()
{
  unsigned int R = 1023, G = 0, B = 0;
  for(unsigned int i = 0; i < 1024; i++)
  {
    if(!MagicalMode)
    {
      return;
    }
    webSocket.loop();
    server.handleClient();

    if(GetLightIntensity() > IntensityThreshold)
    {
      WriteToRgbLed(0, 0, 0);
      continue;
     }
    WriteToRgbLed(R, G, i);
    delay(DelayMs);
  }
}

void DecreaseRed()
{
  unsigned int R = 1023, G = 0, B = 1023;
  for(unsigned int i = 0; i < 1024; i++)
  {
    if(!MagicalMode)
    {
      return;
    }
    webSocket.loop();
    server.handleClient();
    if(GetLightIntensity() > IntensityThreshold)
    {
      WriteToRgbLed(0, 0, 0);
      continue;
    }
    WriteToRgbLed(R - i, G, B);
    delay(DelayMs);
  }
}

void IncreaseGreen()
{
  unsigned int R = 0, G = 0, B = 1023;
  for(unsigned int i = 0; i < 1024; i++)
  {
    if(!MagicalMode)
    {
      return;
    }
    webSocket.loop();
    server.handleClient();
    if(GetLightIntensity() > IntensityThreshold)
    {
      WriteToRgbLed(0, 0, 0);
      continue;
    }
    WriteToRgbLed(R, i, B);
    delay(DelayMs);
  }
}

void DecreaseBlue()
{
  unsigned int R = 0, G = 1023, B = 1023;
  for(unsigned int i = 0; i < 1024; i++)
  {
    if(!MagicalMode)
    {
      return;
    }
    webSocket.loop();
    server.handleClient();
    if(GetLightIntensity() > IntensityThreshold)
    {
      WriteToRgbLed(0, 0, 0);
      continue;
    }
    WriteToRgbLed(R, G, B - i);
    delay(DelayMs);
  }
}

void IncreaseRed()
{
  unsigned int R = 0, G = 1023, B = 0;
  for(unsigned int i = 0; i < 1024; i++)
  {
    if(!MagicalMode)
    {
      return;
    }
    webSocket.loop();
    server.handleClient();
    if(GetLightIntensity() > IntensityThreshold)
    {
      WriteToRgbLed(0, 0, 0);
      continue;
    }
    WriteToRgbLed(i, G, B);
    delay(DelayMs);
  }
}

void DecreaseGreen()
{
  unsigned int R = 1023, G = 1023, B = 0;
  for(unsigned int i = 0; i < 1024; i++)
  {
    if(!MagicalMode)
    {
      return;
    }
    webSocket.loop();
    server.handleClient();
    if(GetLightIntensity() > IntensityThreshold)
    {
      WriteToRgbLed(0, 0, 0);
      continue;
    }
    WriteToRgbLed(i, G - i, B);
    delay(DelayMs);
  }
}

void DoMagicalAction()
{
  if(!MagicalMode)
  {
    return;
  }
  
  auto lightIntensity = GetLightIntensity();
  if(GetLightIntensity() > IntensityThreshold)
  {
     WriteToRgbLed(0, 0, 0);
     return;
  }
  IncreaseBlue();
  DecreaseRed(); 
  IncreaseGreen();
  DecreaseBlue();
  IncreaseRed();
  DecreaseGreen();
}

void setup() 
{
  pinMode(LdrPin, INPUT);
  pinMode(ButtonPin, INPUT);
  pinMode(RedPin, OUTPUT);
  pinMode(GreenPin, OUTPUT);
  pinMode(BluePin, OUTPUT);

  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
    }
  }

  // Connect tp Wifi
  WiFi.mode(WIFI_STA);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.begin(ssid, password);
  }
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Set up mDNS responder:
  if (!MDNS.begin(host)) {
    while(1) { 
      delay(1000);
    }
  }

  //SERVER INIT
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.begin();

  // Needed this to stabilize Websocket connection
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  thread.onRun(DoMagicalAction);
  thread.setInterval(300);
}

void loop() 
{
  if(thread.shouldRun())
  {
    thread.run();
  }
  webSocket.loop();
  server.handleClient();
}
