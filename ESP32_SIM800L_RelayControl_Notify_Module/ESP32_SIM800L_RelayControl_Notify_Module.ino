#include <HardwareSerial.h>

// SIM800L Serial Configuration
#define SIM800_TX 17  // ESP32 GPIO TX2
#define SIM800_RX 16  // ESP32 GPIO RX2
HardwareSerial sim800(1); // Serial1 for SIM800L

// Relay Pin Configuration
#define RELAY_PIN 4    // Relay connected to GPIO4

// Global Variables
String incomingSMS = "";
String senderNumber = "";
bool pumpStatus = false; // false = OFF, true = ON

void setup() {
  Serial.begin(115200);  // Debugging Serial
  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX); // Initialize SIM800L Serial
  pinMode(RELAY_PIN, OUTPUT); // Set relay pin as output
  digitalWrite(RELAY_PIN, LOW); // Turn off relay initially
  
  initializeSIM800(); // Initialize SIM800L
  sendSMS("Module is ON and working correctly.");
}

void initializeSIM800() {
  sendATCommand("AT");           // Check connection
  sendATCommand("AT+CMGF=1");    // Set SMS mode to text
  sendATCommand("AT+CNMI=2,2,0,0,0"); // Immediate SMS display when received
}

void loop() {
  if (sim800.available()) {
    incomingSMS = sim800.readString();
    Serial.println("Received SMS: ");
    Serial.println(incomingSMS);
    
    if (parseSMS(incomingSMS)) {
      if (senderNumber == "+918821861409") {  // Authorized sender
        processSMS();
      } else {
        Serial.println("Unauthorized sender: " + senderNumber);
      }
    }
    incomingSMS = ""; // Clear the buffer
  }
}

bool parseSMS(String sms) {
  senderNumber = "";
  int senderStartIdx = sms.indexOf("+CMT: \"");
  if (senderStartIdx != -1) {
    senderStartIdx += 7; // Move to the sender's number start
    int senderEndIdx = sms.indexOf("\"", senderStartIdx);
    if (senderEndIdx != -1) {
      senderNumber = sms.substring(senderStartIdx, senderEndIdx);
      Serial.println("Sender Number: " + senderNumber);
      
      // Extract message content
      int messageStartIdx = sms.indexOf("\n", senderEndIdx);
      if (messageStartIdx != -1) {
        incomingSMS = sms.substring(messageStartIdx + 1);
        incomingSMS.trim();
        Serial.println("Message Content: " + incomingSMS);
        return true;
      }
    }
  }
  Serial.println("Failed to extract sender number.");
  return false;
}

void processSMS() {
  if (incomingSMS.indexOf("ON") != -1) {
    turnPumpOn();
  } else if (incomingSMS.indexOf("OFF") != -1) {
    turnPumpOff();
  } else if (incomingSMS.indexOf("STATUS") != -1) {
    sendPumpStatus();
  } else {
    Serial.println("Unknown command received.");
  }
}

void turnPumpOn() {
  digitalWrite(RELAY_PIN, HIGH); // Turn on relay
  pumpStatus = true;
  sendSMS("Pump is now ON.");    // Send confirmation
}

void turnPumpOff() {
  digitalWrite(RELAY_PIN, LOW); // Turn off relay
  pumpStatus = false;
  sendSMS("Pump is now OFF.");   // Send confirmation
}

void sendPumpStatus() {
  String statusMessage = pumpStatus ? "Pump is currently ON." : "Pump is currently OFF.";
  sendSMS(statusMessage);
}

void sendSMS(String message) {
  sim800.println("AT+CMGS=\"+918821861409\""); // Replace with your phone number
  delay(1000);
  sim800.println(message); // Send the message
  sim800.write(26);        // ASCII code for Ctrl+Z
  delay(2000);
}

void sendATCommand(String command) {
  sim800.println(command);
  delay(500);
  while (sim800.available()) {
    Serial.println(sim800.readString());
  }
}
