#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPSRedirect.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

//---------------------------------------------------------------------------------------------------------
// Enter Google Script Deployment ID:
const char *GScriptId = "AKfycbxU4B_Au1GGjiIX7rIODEXlPPED5taAdBf75PPRrp3P8q2wqLzpPKR0N1EImQ1MY7og";
//---------------------------------------------------------------------------------------------------------
// Enter network credentials:
const char* ssid     = "TP-Link_2.4G";
const char* password = "Kamanamin42069!";
//---------------------------------------------------------------------------------------------------------
// Google Sheets setup (do not edit)
const char* host        = "script.google.com";
const int   httpsPort   = 443;
const char* fingerprint = "";
String url = String("/macros/s/") + GScriptId + "/exec";
HTTPSRedirect* client = nullptr;
//------------------------------------------------------------
#define RST_PIN  0  // D3
#define SS_PIN   2  // D4
#define BUZZER   4  // D2
//------------------------------------------------------------
MFRC522 mfrc522(SS_PIN, RST_PIN);  // RFID module initialization
//------------------------------------------------------------

/****************************************************************************************************
 * setup Function
****************************************************************************************************/
void setup() {
    Serial.begin(9600);        
    delay(10);
    Serial.println('\n');
    
    SPI.begin();

    // Initialize LCD screen
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connecting to");
    lcd.setCursor(0,1);
    lcd.print("WiFi...");

    // Connect to WiFi
    WiFi.begin(ssid, password);             
    Serial.print("Connecting to ");
    Serial.print(ssid); Serial.println(" ...");
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println('\n');
    Serial.println("WiFi Connected!");
    Serial.println(WiFi.localIP());

    // Use HTTPSRedirect class to create a new TLS connection
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Connecting to");
    lcd.setCursor(0,1);
    lcd.print("Google ");
    delay(4000);
    
    Serial.print("Connecting to ");
    Serial.println(host);

    // Try to connect for a maximum of 5 times
    bool flag = false;
    for(int i=0; i<5; i++){ 
        int retval = client->connect(host, httpsPort);
        if (retval == 1){
            flag = true;
            String msg = "Connected";
            Serial.println(msg);
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(msg);
            delay(2000);
            break;
        } else {
            Serial.println("Connection failed. Retrying...");
        }
    }
    if (!flag){
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Connection fail");
        Serial.print("Could not connect to server: ");
        Serial.println(host);
        delay(5000);
        return;
    }
    delete client;    // delete HTTPSRedirect object
    client = nullptr; // delete HTTPSRedirect object
}

/****************************************************************************************************
 * loop Function
****************************************************************************************************/
void loop() {
    static bool flag = false;
    if (!flag){
        client = new HTTPSRedirect(httpsPort);
        client->setInsecure();
        flag = true;
        client->setPrintResponseBody(true);
        client->setContentTypeHeader("application/json");
    }
    if (client != nullptr){
        if (!client->connected()){
            int retval = client->connect(host, httpsPort);
            if (retval != 1){
                Serial.println("Disconnected. Retrying...");
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Disconnected.");
                lcd.setCursor(0,1);
                lcd.print("Retrying...");
                return;
            }
        }
    } else {
        Serial.println("Error creating client object!");
    }

    // Fetch available PCs before scanning for RFID
    String payload_fetch = "{\"command\": \"get_data\", \"sheet_name\": \"Sheet1\"}";
    client->POST(url, host, payload_fetch);

    String sheetData = client->getResponseBody();  // Parse response from Google Sheets
    int availablePCs = parseAvailablePCs(sheetData);  // Count available PCs
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Available PC: ");
    lcd.print(availablePCs);
    lcd.setCursor(0, 1);
    lcd.print("  Scan your ID");

    /* Initialize MFRC522 Module */
    mfrc522.PCD_Init();
    /* Look for new cards */
    if (!mfrc522.PICC_IsNewCardPresent()) { return; }
    /* Select one of the cards */
    if (!mfrc522.PICC_ReadCardSerial()) { return; }

    // Convert UID to String
    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        uidString += String(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println("Card UID: " + uidString);

    // Check if the card is already registered with a PC
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Checking...");

    // Payload for checking PC status for UID
    String payload_check = "{\"command\": \"check_uid\", \"sheet_name\": \"Sheet1\", \"uid\": \"" + uidString + "\"}";
    client->POST(url, host, payload_check);
    String checkResponse = client->getResponseBody();
    Serial.println("UID Check Response: " + checkResponse);

    DynamicJsonDocument checkDoc(512);
    deserializeJson(checkDoc, checkResponse);

    bool isReserved = checkDoc["isReserved"];
    String pcNumber = checkDoc["pcNumber"].as<String>();

    if (isReserved) {
        // Release the PC if already reserved by this UID
        String payload_update = "{\"command\": \"update_row\", \"sheet_name\": \"Sheet1\", \"uid\": \"" + uidString + "\", \"status\": \"Available\"}";
        client->POST(url, host, payload_update);
        String releaseResponse = client->getResponseBody();
        Serial.println("Release Response: " + releaseResponse);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Logged Out on:");
        lcd.setCursor(0,1);
        lcd.print(pcNumber);
        delay(2000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(pcNumber);
        lcd.print(" is now");
        lcd.setCursor(0,1);
        lcd.print("available.");
        delay(2000);

    } else {
        // Reserve a PC for this UID
        String payload_reserve = "{\"command\": \"reserve_pc\", \"sheet_name\": \"Sheet1\", \"uid\": \"" + uidString + "\"}";
        client->POST(url, host, payload_reserve);
        String reserveResponse = client->getResponseBody();
        Serial.println("Reserve Response: " + reserveResponse);

        DynamicJsonDocument reserveDoc(512);
        deserializeJson(reserveDoc, reserveResponse);

        if (reserveDoc["status"] == "success") {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Reserved PC: ");
            lcd.setCursor(0, 1);
            lcd.print(reserveDoc["pcNumber"].as<String>());
            delay(2000);
        } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("No PC Available");
            delay(2000);
        }
    }
}

/****************************************************************************************************
 * parseAvailablePCs() function
 * Parses Google Sheets data and returns the count of available PCs
****************************************************************************************************/
int parseAvailablePCs(String sheetData) {
    int availablePCs = 0;

    // Deserialize the JSON data
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, sheetData);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return 0;
    }

    // Access the PCs array
    JsonArray PCs = doc["PCs"].as<JsonArray>();

    for (JsonObject pc : PCs) {
        if (strcmp(pc["status"], "Available") == 0) {
            availablePCs++;
        }
    }

    return availablePCs;
}
