#define HOSTNAME "esp32_nt_1"
#define LOGS_ENDPOINT "https://log-api.newrelic.com/log/v1"

#include "project_credentials.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>


#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32_Servo.h>
#include <Wire.h>
#include "MFRC522_I2C.h"

Servo myservo;
WiFiMulti WiFiMulti;
LiquidCrystal_I2C mylcd(0x27, 16, 2);
const char* NR_API_KEY = NEWRELIC_API_KEY;

unsigned long lastHB = 0;

#define gasPin 23
#define buzPin 25
boolean i = 1;

MFRC522 mfrc522(0x28);  // create MFRC522 at i2c address 0x28.
#define servoPin 13
#define btnPin 16
#define MOISTURE_PIN 34
#include "xht11.h"


boolean btnFlag = 0;


xht11 xht(17);
unsigned char dht[4] = { 0, 0, 0, 0 };  //Only the first 32 bits of data are received, not the parity bits

//Making RFID read password global for memory management
String password = "";

void setClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print(F("Waiting for NTP time sync: "));
  time_t nowSecs = time(nullptr);
  while (nowSecs < 8 * 3600 * 2) {
    delay(500);
    Serial.print(F("."));
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  struct tm timeinfo;
  gmtime_r(&nowSecs, &timeinfo);
  Serial.print(F("Current time: "));
  Serial.print(asctime(&timeinfo));
}



void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Waiting for WiFi to connect...");
  while ((WiFiMulti.run() != WL_CONNECTED)) {
    Serial.print(".");
  }
  Serial.println(" connected");
}


unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

void sendNrEvent(char* eventType, char* value)

{

  WiFiClientSecure* client = new WiFiClientSecure;
  if (client) {



    String jsonString = "";
    client->setInsecure();
    //client -> setCACert(rootCACertificate);

    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
      HTTPClient https;
      Serial.print("[HTTPS] begin...\n");

      //Send NR

      if (https.begin(*client, "https://insights-collector.newrelic.com/v1/accounts/2506584/events")) {  // HTTPS
        Serial.print("[HTTPS] POST...\n");

        //Build NR Event JSON
        StaticJsonDocument<384> doc;
        doc["eventType"] = eventType;
        doc["timestamp"] = getTime();
        doc["value"] = value;
        doc["hostname"] = HOSTNAME;

        serializeJson(doc, jsonString);

        Serial.printf(jsonString.c_str());

        // start connection and send HTTP header
        https.addHeader("Api-Key", NR_API_KEY);
        https.addHeader("Content-Type", "application/json");
        int httpCode = https.POST(jsonString);

        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("\n[HTTPS] GET... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            Serial.println(payload);
          }
        } else {
          Serial.printf("\n[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }

        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }

      // End extra scoping block
    }


    delete client;
  } else {
    Serial.println("Unable to create client");
  }
}

void sendNrLog(const char* message)

{

  WiFiClientSecure* wifiClient = new WiFiClientSecure;
  if (wifiClient) {


         //Build NR Log JSON
        String jsonString = "";
        StaticJsonDocument<384> jsonDoc;
        jsonDoc["timestamp"] = getTime();
        jsonDoc["message"] = message;
        jsonDoc["logtype"] = "iotlog";
        jsonDoc["hostname"] = "esp32_nt_1";
        serializeJson(jsonDoc, jsonString);
  
    //We are not checking certificate for now
    wifiClient->setInsecure();

    { //Scoping block for memory management since HTTP has heavy mem usage
  
      HTTPClient httpsClient;
      Serial.print("[HTTPS] attempting connection...\n");

      if (httpsClient.begin(*wifiClient, LOGS_ENDPOINT)) {  
        Serial.print("[HTTPS] POST...\n");
        Serial.printf(jsonString.c_str());
        httpsClient.addHeader("Api-Key", NR_API_KEY);
        httpsClient.addHeader("Content-Type", "application/json");
        int responseCode = httpsClient.POST(jsonString);

        if (responseCode > 0) {
          Serial.printf("\n[HTTPS] GET... code: %d\n", responseCode);

          if (responseCode == HTTP_CODE_OK || responseCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String responseBody = httpsClient.getString();
            Serial.println(responseBody);
          }
        } else {
          Serial.printf("\n[HTTPS] GET... failed: %s\n", httpsClient.errorToString(responseCode).c_str());
        }

        httpsClient.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }

     //End memory management scoping block
    }


    delete wifiClient;
  } else {
    Serial.println("Unable to create client");
  }
}

void ShowReaderDetails() {
  //  attain the MFRC522 software
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("MFRC522 Software Version: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (unknown)"));
  Serial.println("");
  // when returning to 0x00 or 0xFF, may fail to transmit communication signals
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("WARNING: Communication failure, is the MFRC522 properly connected?"));
  }
}


void setup() {
  Serial.begin(115200);  // initialize and PC's serial communication


  mylcd.init();
  mylcd.backlight();
  mylcd.clear();
  mylcd.println("Connecting WIFI");

  connectWifi();
  setClock();

  Wire.begin();         // initialize I2C
  mfrc522.PCD_Init();   // initialize MFRC522
  ShowReaderDetails();  // display PCD - MFRC522 read carder
  Serial.println(F("Scan PICC to see UID, type, and data blocks..."));
  myservo.attach(servoPin);
  pinMode(btnPin, INPUT);
  mylcd.setCursor(0, 0);
  mylcd.clear();
  mylcd.print("Card");

  pinMode(MOISTURE_PIN, INPUT);

  sendNrLog("Initialization Complete...");
  sendNrEvent("IoTHeartBeat", "Init");
  sendNrEvent("doorState", "closed");

}

// Periodically called function to check env conditions
// Will always read VOC gas detector and only read other ENV during heartbeat interval
void checkEnv(boolean hb) {

  boolean gasVal = digitalRead(gasPin);
  if (gasVal == 0) {
    sendNrLog("Dangerous Gas Detected!");
    sendNrEvent("VOC", "1");
  }

  //Read other env if HB interval
  if (hb) {

    //already sent above if gas detected
    if(gasVal == 1) {
      sendNrEvent("VOC", "0");
    }

    char moistureDetected = 0;
    int moistureValue = analogRead(MOISTURE_PIN);
    Serial.print("Moisure value: ");
    Serial.println(moistureValue);
      if(moistureValue > 1500) {
        moistureDetected = 1;
  }

    if(moistureDetected)   {
      sendNrEvent("Moisture", "1");
    }
    else {
      sendNrEvent("Moisture", "0");
    }

    //Read Temp / Humidity
    if (xht.receive(dht)) {  

      char tempString[5];
      float tempFloat = dht[3] / 10.0 + dht[2];
      tempFloat *= 9.0;
      tempFloat /= 5.0;
      tempFloat += 32;

      sprintf(tempString, "%0.1f", tempFloat);

      Serial.print("RH:");
      Serial.print(dht[0]);  //The integral part of humidity, DHT [1] is the fractional part
      Serial.print("%  ");
      Serial.print("Temp:");
      Serial.print(dht[2]);
      Serial.print(".");
      Serial.println(dht[3]);  //The integral part of temperature, DHT [3] is the fractional part
      Serial.println("C");

      mylcd.setCursor(0, 0);
      mylcd.print("T = ");
      mylcd.print(tempString);
      mylcd.setCursor(0, 1);
      mylcd.print("H = ");
      mylcd.print(dht[0]);

      sendNrEvent("Temperature", tempString);

      sprintf(tempString, "%u", dht[0]);
      sendNrEvent("Humidity", tempString);
      
      
    } else {  //Read error
      Serial.println("sensor error");
      sendNrLog("Cannot communicate to temperature sensor.");
    }
  }
}

void loop() {

  if (getTime() - lastHB > 120) {
    sendNrEvent("IoTHeartBeat", "loop");
    lastHB = getTime();
    checkEnv(true);
  }

  checkEnv(false);

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    password = "";
    if (btnFlag == 1) {
      boolean btnVal = digitalRead(btnPin);
      if (btnVal == 0)  //Swipe the card to open the door and click button 1 to close the door
      {
        sendNrLog("Pin 16 button press...");
        Serial.println("close");
        mylcd.setCursor(0, 0);
        mylcd.print("close");
        myservo.write(0);
        sendNrLog("Closing door...");
        sendNrEvent("doorState", "closed");
        btnFlag = 0;
      }
    }
    return;
  }

  // select one of door cards. UID and SAK are mfrc522.uid.

  // save UID
  Serial.print(F("Card UID:"));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    //Serial.print(mfrc522.uid.uidByte[i], HEX);
    Serial.print(mfrc522.uid.uidByte[i]);
    password = password + String(mfrc522.uid.uidByte[i]);
  }
  Serial.println();
  Serial.print("Password: ");
  Serial.println(password);
  if (password == RFID_CARD_PASSWORD)  //The card number is correct, open the door
  {
    Serial.println("open");
    mylcd.setCursor(0, 0);
    mylcd.clear();
    mylcd.print("open");
    sendNrLog("Opening Door...");
    myservo.write(180);
    sendNrEvent("doorState", "open");

    String message = "Good Card: RFID" + password;
    sendNrLog(message.c_str());
    sendNrLog("Door opening...");
    password = "";
    btnFlag = 1;
  } 
  
  else {

    mylcd.setCursor(0, 0);
    mylcd.print("error");

    String message = "Bad Card: RFID" + password;
    password = "";
    sendNrLog(message.c_str());
  }
}


