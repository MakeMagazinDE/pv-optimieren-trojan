
/*

Program:  Transmitter-E
Author:   Walter Trojan
Date:     1Q 2025

Purpose:  Evaluation of a digital Electricity meter 
*           using actual power consumption and total meter count
*         Receives one data package per second
*         Sends data via TCP server
*         Publishes Data via Web server
*
*/

// Include Libraries
#include <ESP32Time.h>
#include <WiFi.h>
#include "time.h"
#include <WebServer.h>


#define RXD2 16
#define TXD2 17
#define LEDGN 12      // WLAN OK
#define LEDGE 13      // Receives data block

// TCP server
WiFiServer tcpServer(81); // Use port 81 for TCP server
WiFiClient tcpClient;

// Web server
WebServer webServer(80); // Use port 80 for web server

ESP32Time rtc;


// Define a data structure
typedef struct struct_message {
  char tim[17];         // Date and time
  float z;              // total meter count
  float l;              // actual power
} struct_message;
 
// Create a structured object
struct_message myData;

struct tm timeinfo;
 
 
byte bt;
byte msg [2000];
int  iz,iy,i,sta;

char z[2];
int itag;
String tim;


int32_t ikwges;
int64_t ikwzae;
float   kwges;
float   kwzae;

// WLAN data
const char* ssid = "xxxxxxxxxxxx";          // Put your data here
const char* password = "yyyyyyyyyyyyyy";

const char* ntpServer = "pool.ntp.org";     // time server
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;



void setup() {
  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);  // port meter adapter
  Serial.println("Serial Txd is on pin: "+String(TXD2));
  Serial.println("Serial Rxd is on pin: "+String(RXD2));

  pinMode(LEDGN, OUTPUT);
  pinMode(LEDGE, OUTPUT);
  digitalWrite(LEDGN, LOW);       // LEDs off
  digitalWrite(LEDGE, LOW); 

  sta = 0;
  iz = 0;
 
  // Connect to Wi-Fi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_AP_STA);           // Set ESP32 as a Wi-Fi Station and Accesspoint
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

   // Start TCP server
  tcpServer.begin();
  Serial.println("TCP server started");

  // Start web server
  webServer.on("/", handleRoot); // Handle root URL
  webServer.on("/data", handleData); // Handle AJAX request for sensor data
  webServer.begin();
  Serial.println("Web server started");  
  
  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo))
    Serial.println("Failed to obtain time");  
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  if (getLocalTime(&timeinfo)){
   rtc.setTimeStruct(timeinfo); 
  }

  delay(1000);
  digitalWrite(LEDGN, HIGH);      // WLAN-LED on
  
   Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    
}

void loop() {
  switch (sta) {  // state-machine
    case 0:{      // Start new Message
      iz = 0;
      iy = 0;
      sta = 1;
      vTaskDelay(50 / portTICK_PERIOD_MS);
      break;  
    }

    case 1:{      // Seek begin of data block
      char sa [] = {0x1B,0x1B,0x1B,0x1B,0x01,0x01,0x01,0x01};
      while (Serial2.available()) {
        bt = Serial2.read(); 
        msg[iy] = bt;
      
        if (bt == sa[iz]){
          iy++;
          iz++;
        }
        else{
          iy = 0;
          iz = 0;
        }

        if (iz == 8){
          iz = 0;
          sta = 2;
          Serial.println();
          Serial.println("Start neue Messung");
          digitalWrite(LEDGE, HIGH);            // Packet-LED on
          break;
        }
      }
      break;  
    }

    case 2:{      // Read Message-Body 
      char se [] = {0x1B,0x1B,0x1B,0x1B,0x1A};
      while (Serial2.available()) {
        bt = Serial2.read(); 
        msg[iy] = bt;
        iy++;
        
        if (bt == se[iz]){
          iz++;
        }
        else{
          iz = 0;
        }

        if (iz == 5){
          iz = 0;
          sta = 3;
          break;
        }
      }
      break; 
    }

    case 3:{      // Find Message-End (last 3 bytes)
      while (Serial2.available()) {
        bt = Serial2.read(); 
        msg[iy] = bt;
        iy++;
        iz++;
        if (iz == 3){
          sta = 4;
          break;
        } 
      }
      break;  
    } 

    case 4:{      // output Message 
      Serial.print("Anzahl Werte: ");
      Serial.println(iy); 
      Serial.println();

      for (i=0;i<iy;i++){
        Serial.printf(" %02X",msg[i]);       
        if ((i+1) % 32 == 0)        // 32 Bytes per line (modulo)
          Serial.println();
      }  
      //Serial.println();
      vTaskDelay(50 / portTICK_PERIOD_MS);
      digitalWrite(LEDGE, LOW);     // Packet-LED off
      sta = 5;
      break;  
    }   

    case 5:{      // Find total meter count
      byte such [] = {0X01 , 0X00 , 0X01 ,  0X08, 0X00, 0XFF};  // label for count
      int sulen,isu,mgsiz,j;
      bool suok;

      mgsiz = iy;
      sulen = sizeof(such);
      suok  = false;
 
      for (i=0; i<mgsiz-sulen; i++){
        if (msg[i] == such[0]){
          suok = true;
          for (j=1; j < sulen; j++){
            if (msg[i+j] != such[j]){
              suok = false;
              break;
            }
          }
        }
        if (suok){    // convert meter count to decimal
          ikwzae =  (uint64_t)msg[i+18]  << 40;
          ikwzae |= (uint64_t)msg[i+19]  << 32;
          ikwzae |= (uint64_t)msg[i+20]  << 24;
          ikwzae |= (uint64_t)msg[i+21]  << 16;
          ikwzae |= (uint64_t)msg[i+22]  << 8;
          ikwzae |= (uint64_t)msg[i+23];
          kwzae   = (float)ikwzae/100000000;

          Serial.println();
          Serial.print("Zählerstand = ");
          Serial.print(kwzae);
          Serial.println(" kwh");
          break;
        }
      } 

                      // Find actual power
      suok = false;
      byte suchw [] = {0X01 , 0X00 , 0X10 ,  0X07, 0X00, 0XFF};  // label for power
      sulen = sizeof(suchw);
      for (i=0; i<mgsiz-sulen; i++){
        if (msg[i] == suchw[0]){
          suok = true;
          for (j=1; j < sulen; j++){
            if (msg[i+j] != suchw[j]){
              suok = false;
              break;
            }
          }
        } 
        if (suok){    // Convert power to decimal
          ikwges =  (uint32_t)msg[i+13] << 24;
          ikwges |= (uint32_t)msg[i+14] << 16;
          ikwges |= (uint32_t)msg[i+15] << 8;
          ikwges |= (uint32_t)msg[i+16];
          kwges   = (float)ikwges/100;

          Serial.print("Leistung = ");
          Serial.print(kwges);
          Serial.println(" W");
          break;
        }

      } 
      Serial.println(); 
      vTaskDelay(50 / portTICK_PERIOD_MS);
      sta = 9;
      break;  
    }


    case 9:{      // Transfer to TCP and Web clients
      Serial.println("Ende der Messung");
      Serial.println();
      //while (true); 

      myData.z = kwzae;
      myData.l = kwges; 

      tim = rtc.getTime("%d.  .%Y %H:%M");        // 16.  .2024 13:05
      itag = rtc.getMonth() + 1;
      itoa(itag,z,10);
      if (itag < 10){                             // 1..9
        tim[3] = '0';
        tim[4] = z[0];
      }
      else{                                       // 10..12
        tim[3] = z[0];
        tim[4] = z[1];
      }
      
      tim.toCharArray(myData.tim, 17);

      handleTcpServer();            // Handle TCP server
  
      webServer.handleClient();     // Handle web server

      Serial.println("Ende Messung");
      Serial.println(myData.tim);
      Serial.println(myData.z);
      Serial.println(myData.l);
                  
      sta = 0;
      break;  
    }
       
  }
}

// Handle TCP server
void handleTcpServer() {
  Serial.println("Start TCP-Server");
  if (!tcpClient || !tcpClient.connected()) {
    tcpClient = tcpServer.available(); // Accept new client
    if (tcpClient) {
      Serial.println("New TCP client connected");
    }
  }

  if (tcpClient && tcpClient.connected()) {
    // Send data to TCP client
    tcpClient.write((byte*)&myData, sizeof(myData)); // Send data as bytes
    Serial.println("Sent data to TCP client: " + String(myData.tim));

  }
}

// dynamic Web page
void handleRoot() {
  Serial.println("Start Web-Server");
  String html = R"(
    <html>
      <head>
        <title>ESP32 Web Server</title>
        <style>
          body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin-top: 50px;
            background-color: #f0f0f0;
          }
          h1 {
            color: #444;
            font-size: 3em;
            margin-bottom: 20px;
          }
          p {
            font-size: 3em;
            margin: 20px 0;
            color: #555;
            font-weight: bold; /* Make data bold */
          }
          .data-container {
            display: inline-block;
            text-align: left;
            margin-top: 20px;
          }
          .data-value {
            font-size: 1.5em; /* Larger font size for data */
            font-weight: bold; /* Make data bold */
            color: #000; /* Darker color for better visibility */
          }
        </style>
        <script>
          setInterval(function() {
            fetch("/data")
              .then(response => response.json())
              .then(data => {
                document.getElementById("tim").innerText = data.tim;
                document.getElementById("z").innerText = data.z;
                document.getElementById("l").innerText = data.l;
              });
          }, 1000);
        </script>
      </head>
      <body>
        <h1>Zaehler von Musterman</h1>
        <div class="data-container">
          <p>Datum/Zeit: <span id="tim" class="data-value">)" + String(myData.tim) + R"(</span></p>
          <p>Zaehlerstand: <span id="z" class="data-value">)" + String(myData.z) + R"(</span></p>
          <p>Leistung: <span id="l" class="data-value">)" + String(myData.l) + R"(</span></p>
        </div>
      </body>
    </html>
  )";

  webServer.send(200, "text/html", html);
  Serial.println("Served web page to client");
}

// Handle AJAX request for sensor data
void handleData() {
  // Create a JSON response with the struct_message data
  String jsonResponse = "{";
  jsonResponse += "\"tim\":\"" + String(myData.tim) + "\",";
  jsonResponse += "\"z\":" + String(myData.z) + ",";
  jsonResponse += "\"l\":" + String(myData.l);
  jsonResponse += "}";

  webServer.send(200, "application/json", jsonResponse);
  Serial.println("Served sensor data to web client");
}
