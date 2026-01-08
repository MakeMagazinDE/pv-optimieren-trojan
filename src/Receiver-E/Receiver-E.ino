/*
Program:  Receiver-E
Author:   Walter Trojan
Date:     1Q 2025

Purpose:  Receives one data package per second from Transmitter-E
*         Receives data via TCP server
*         Displays data on LCD
*         Stores data on SD-card one file each day
*/

 
// Include Libraries
#include <WiFi.h>
#include <Arduino.h>
#include <Wire.h>

#include <floatToString.h>
#include <monitor_printf.h>
#include "SPI.h"

#include "FS.h"
#include "SD.h"

#define SD_CS 5

// display grafic driver
#define LGFX_AUTODETECT         
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>  
#include <stdio.h>

// WiFi credentials
const char* ssid = "xxxxxxxxxxxx";    // put your WiFi data here
const char* password = "yyyyyyyyyyyyyy";


// Server IP and port
const char* serverIP = "192.168.xxx.yyy"; // Replace with the transmitter's IP address
const int serverPort = 81;

WiFiClient tcpClient;


static LGFX lcd;

int sck = 18;                   // SPI-bus
int miso = 19;
int mosi = 23;
int cs = 5;                     // SD-Card CS 

 
// Define a data structure
typedef struct struct_message {
  char tim[17] = "00.00.0000 00:00";    // Date and time
  float z;                              // total meter count
  float l;                              // actual power consumption
} struct_message;
 
// Create a structured object
struct_message myData;
 
char zst[8];  
char lst[8];
bool inrec = false;
int  msgnr = 0;
float oldz=0,oldl=0;
char oldtim[17] = "01.01.2024 00:00";
int  i,j;

bool evmin  = false;
bool evstd  = false;
bool evtag  = false;

float minsum = 0;       // Sum power per minute
int   mincnt = 0;       // Number inputs per minute
int   minpos = 0;       // Horizontal position
int   minhih = 0;       // Value pixel (0..150)
int   mincol = 0;       // Color of value gn, rt as > 150
int   minidx = 0;       // Index of grafdat
int   grafdat[121];     // Values for 120 min.

bool  newnam = true;    // New file name 
int   xs;
int   ys;

String datz;
char fnam[15];
char datc[32];
char sumc[32];
String s1 = ",";
String s2 = "\n";
char datarr[61][32];
bool datok = true;
int  datidx = 0;
char testmsg[14] = "testSD 123456";


void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  // Connect to the TCP server
  if (tcpClient.connect(serverIP, serverPort)) {
    Serial.println("Connected to TCP server");
  } else {
    Serial.println("Connection to TCP server failed");
  }  

   
  SPI.begin(sck, miso, mosi, cs);       // Start SD Card
  delay(100);
  if (!SD.begin(cs)) {
    Serial.println("Card Mount Failed");
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
  }
  
  if(not SDchck(SD)){                   // No recording if no card
    Serial.println("SD-Fehler, keine Datenaufzeichnung");
    datok = false;
  }
  else{
    Serial.println("Datenaufzeichnung startet"); 
    datok = true;
  }

  lcd.init();                           // Start Grafik
  lcd.setRotation(1);
  lcd.setBrightness(255);               // max brightness

  Serial.println(lcd.width());
  Serial.println(lcd.height());  

  drawBasis();

  inrec = false;
  msgnr = 0;
  //while(true);
}
 
void loop() {
  if (tcpClient.connected() && tcpClient.available()) {
    tcpClient.readBytes((byte*)&myData, sizeof(myData)); // Read data as bytes

    inrec = true;
    msgnr++; 
    Serial.print("Msg in: "); 
    Serial.println(msgnr);
    Serial.println("Time: " + String(myData.tim));
    Serial.println("Z: " + String(myData.z));
    Serial.println("L: " + String(myData.l));  
    Serial.println(); 

  if(msgnr == 1)
    strcpy(oldtim,myData.tim);

  char c1,c2;
  c1 = myData.tim[15];
  c2 = oldtim[15];
  if(c1 != c2)
    evmin = true;

  c1 = myData.tim[12];
  c2 = oldtim[12];
  if(c1 != c2)
    evstd = true;  

 
  c1 = myData.tim[01];
  c2 = oldtim[01];
  if(c1 != c2)
    evtag = true;
  } 
  else if (!tcpClient.connected()) {
    Serial.println("Disconnected from server. Attempting to reconnect...");
    if (tcpClient.connect(serverIP, serverPort)) {
      Serial.println("Reconnected to TCP server");
    } else {
      Serial.println("Reconnection failed");
    }
  }  
 
  if (inrec) {                          // New message arrived

    mincnt++;                           // Average power
    minsum = minsum + myData.l;

    if(evmin){                          // New minute
      Serial.println("evmin");
      evmin = false;

      minsum = minsum / mincnt;         // Average 60 seconds
      minhih = minsum / 10;             // Scale down to 0..150
      grafdat[minidx] = minhih;         // Save grafic values for move
      if (minhih > 150) {               // If > 150 (1500W) color red
        minhih = 150;
        mincol = TFT_RED;
      } 
      else
        mincol = TFT_GREEN;

      minidx++;                         // new minute

      if(minidx == 119){                // Grafic move 5 positions back
        for(minidx=1; minidx<115; minidx++)
          grafdat[minidx] = grafdat[minidx+5];
        minidx = 0;                     
        minpos = 0;                     
        
        drawBasis();                    // Draw basic picture

        for(minidx=1; minidx<114; minidx++){
          minhih = grafdat[minidx];
          if (minhih > 150) {
            minhih = 150;
            mincol = TFT_RED;
          } 
          else
            mincol = TFT_GREEN;
          minpos++;                     // Dual line per minute                    
          drawWert(minpos,minhih,mincol);
          minpos++; 
          drawWert(minpos,minhih,mincol);          
        }
        lcd.drawFastHLine(xs, ys+50, 242, 0xFFFFFFU);  // new axis
        lcd.drawFastHLine(xs, ys+100, 242, 0xFFFFFFU); 
        lcd.drawFastVLine(xs+120, ys, 152, 0xFFFFFFU);
      }
      else{                             // Operation without grafic move
        minpos++;                       // Dual line
        drawWert(minpos,minhih,mincol);
        minpos++; 
        drawWert(minpos,minhih,mincol);
        if(minhih > 150){
          lcd.drawFastHLine(xs, ys+50, 242, 0xFFFFFFU);  // new axis
          lcd.drawFastHLine(xs, ys+100, 242, 0xFFFFFFU);           
        }
       }
      

      if(datok and (datidx < 60)){         // collect minute values
        datidx++;    
        floatToString(myData.z, zst, sizeof(zst),0); // Format data record
        floatToString(myData.l, lst, sizeof(lst),0);        
        datz = myData.tim;
        datz[10] = ',';
        datz = datz + s1 + zst + s1 + lst + s2;
        Serial.println(datz);
        Serial.println();
        datz.toCharArray(sumc, sizeof(sumc)); 
        memcpy(&datarr[datidx][0], &sumc, sizeof(sumc)); 
        Serial.println(datidx);
        if(datidx == 60){
          evstd = true;
        }
      }
      minsum = 0;
      mincnt = 0;
    }
    

    if(evstd){                           // New hour, save 60 minutes
      Serial.println("evstd");
      evstd = false;
      if(newnam){                        // If new data name during start or new day
        strcpy(fnam, "/");
        strncat(fnam, myData.tim, 10);
        strcat(fnam, ".txt");
        fnam[3] = '_';
        fnam[6] = '_'; 
        newnam = false;
      }          
      putstd(SD, fnam, datidx);          // Save 60 Values
      Serial.println(fnam);

      datidx = 0;
      Serial.println(datarr[1]);
    }
     
    if(evtag){                           // New day, new filename
      Serial.println("evtag");
      newnam = true;
      evtag = false;
    } 
 
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);    
    lcd.setCursor(80, 0);               // Draw values
    lcd.print(myData.tim);
    lcd.setCursor(80, 20);
    lcd.print("           ");
    lcd.setCursor(80, 20);
    lcd.print(myData.z, 1);    
    if(myData.l == 0){
      lcd.setCursor(240, 20);  
      lcd.print("0          ");
    }            
    else{
      lcd.setCursor(240, 20);
      lcd.print("           ");
      lcd.setCursor(240, 20);
      lcd.print(myData.l, 1);
    }
    strcpy(oldtim, myData.tim);

    inrec = false;
  }
  
  //delay(100);
  vTaskDelay(100 / portTICK_PERIOD_MS);
  
}

// Utility procedures

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}


void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char *path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file) {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len) {
      size_t toRead = len;
      if (toRead > 512) {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %lu ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
  file.close();
}

bool SDchck(fs::FS &fs){
  File file = fs.open("/testSD.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }  
  if (file.print(testmsg)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
    return false;    
  }
  file.close();  
  
  return true;  
    
}

bool putstd(fs::FS &fs, const char *path, int di){
  File file = fs.open(path);
  if (!file) {
    File file = fs.open(path, FILE_WRITE);
    file.print(path);
    file.println("  Start Aufzeichnung");
    file.close();     
  }

  file = fs.open(path, FILE_APPEND);
  for (int i = 1; i < di; i++){
    file.println(datarr[i]);
  }
  file.close();
  Serial.println("Stunde geschrieben");
  return true;
}

void drawBasis(){
  xs = 60;                              // Basic koordinates Grafik
  ys = 65;

  lcd.fillRect(0, 0, 320, 240, TFT_BLACK);
  lcd.setTextColor(0xFFFFFFU, 0x000000U);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setCursor(0, 0); 
  lcd.print("Zeit: ");
  lcd.setCursor(0, 20); 
  lcd.print("ZaehlStd: ");
  lcd.setCursor(160, 20); 
  lcd.print("Leistung: ");  
  lcd.drawRect(xs, ys, 242, 152, 0xFFFFFFU);
  lcd.drawFastVLine(xs+120, ys, 152, 0xFFFFFFU);
  lcd.drawFastHLine(xs, ys+50, 242, 0xFFFFFFU);  
  lcd.drawFastHLine(xs, ys+100, 242, 0xFFFFFFU); 

  lcd.setCursor(xs-30, ys-4);
  lcd.print("1.5");
  lcd.setCursor(xs-30, ys+46);
  lcd.print("1.0");
  lcd.setCursor(xs-30, ys+96);
  lcd.print("0.5");
  lcd.setCursor(xs-15, ys+146);
  lcd.print("0");

  lcd.setCursor(xs, ys-20);
  lcd.print("-2h");
  lcd.setCursor(xs+110, ys-20);
  lcd.print("-1h");
  lcd.setCursor(xs+230, ys-20);
  lcd.print("0h");
}

void drawWert(int16_t xp, int16_t hp, uint16_t cp){
  //Serial.println(hp);
  if(hp > 0){
    lcd.drawFastVLine(xs+xp, ys+151-hp, hp, cp);            // Value gn or rt
    lcd.drawFastVLine(xs+xp+1, ys+151-hp, hp, cp); 
  }
  else{
    lcd.drawFastVLine(xs+xp, ys+152, ys+153, TFT_BLUE);     // Zero line blue
    lcd.drawFastVLine(xs+xp+1, ys+152, ys+153, TFT_BLUE);     
  }
}
