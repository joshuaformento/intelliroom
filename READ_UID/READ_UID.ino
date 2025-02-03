#include <SPI.h>
#include <MFRC522.h>

const uint8_t RST_PIN = D3;  // Reset pin for ESP8266 (adjust if necessary)
const uint8_t SS_PIN = D4;   // Slave Select pin for ESP8266 (adjust if necessary)

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

void setup() {
  Serial.begin(9600);       // Initialize serial communications with the PC
  SPI.begin();              // Initialize SPI bus
  mfrc522.PCD_Init();       // Initialize MFRC522
  Serial.println("Scan an RFID card...");
}

void loop() {
  // Check if a new card is present
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Print the UID of the card as numbers
  Serial.print("Card UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i]);
    if (i < mfrc522.uid.size - 1) {
      Serial.print("");  // Optional: Add a separator between bytes
    }
  }
  Serial.println();

  // Halt the current card so another one can be read
  mfrc522.PICC_HaltA();
}
