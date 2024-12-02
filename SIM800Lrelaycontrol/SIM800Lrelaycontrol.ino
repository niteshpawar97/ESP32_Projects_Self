#include <HardwareSerial.h>

// SIM800L Serial Configuration
#define SIM800_TX 17  // ESP32 GPIO TX2
#define SIM800_RX 16  // ESP32 GPIO RX2
HardwareSerial sim800(1); // Serial1 for SIM800L

// Pin Definitions
#define RELAY_PIN 4       // Relay connected to GPIO4
#define FAULT_PIN 32      // Fault light connected to GPIO32 (Red Light)

// Global Variables
bool pumpStatus = false;  // false = OFF, true = ON
bool faultStatus = false; // false = No Fault, true = Fault Detected
String callerNumber = ""; // Stores the incoming caller's number
unsigned long autoAnswerDelay = 3000; // Delay in milliseconds before auto-answering (3 seconds)

void setup() {
  Serial.begin(115200);  // Debugging Serial
  Serial.println("Initializing system...");

  sim800.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX); // Initialize SIM800L Serial
  Serial.println("SIM800L initialized.");

  pinMode(RELAY_PIN, OUTPUT);     // Set relay pin as output
  pinMode(FAULT_PIN, INPUT);      // Configure GPIO32 as Input without Pull-Up
  digitalWrite(RELAY_PIN, LOW);   // Turn off relay initially
  Serial.println("Relay and fault pin configured.");

  initializeSIM800(); // Initialize SIM800L
  Serial.println("SIM800L configured.");
  sendSMS("System is ON and monitoring faults.", "+918821861409"); // System startup message
}

void initializeSIM800() {
  sendATCommand("AT");           // Check connection
  sendATCommand("AT+CMGF=1");    // Set SMS mode to text
  sendATCommand("AT+CNMI=2,2,0,0,0"); // Immediate SMS display when received
  sendATCommand("AT+CLIP=1");    // Enable Caller ID Notification
  sendATCommand("AT+DDET=1");    // Enable DTMF detection
  Serial.println("DTMF detection enabled.");
}

void loop() {
  monitorFault();  // Continuously check for fault condition

  if (sim800.available()) {
    String response = sim800.readString();
    Serial.println("SIM800L Response: ");
    Serial.println(response);

    // Handle incoming call and extract caller number
    if (response.indexOf("RING") != -1) {
      Serial.println("Incoming call detected.");
      extractCallerNumber(response); // Extract caller number
      autoAnswerCall(); // Automatically answer the incoming call
    }

    // Handle DTMF tones during call
    if (response.indexOf("+DTMF:") != -1) {
      Serial.println("DTMF Tone Detected.");
      handleDTMF(response);
    }
  }
}

void monitorFault() {
  int faultSignal = digitalRead(FAULT_PIN);  // Read fault pin status
  
  if (faultSignal == HIGH && !faultStatus) {  // Fault detected (Red Light ON)
    faultStatus = true;
    Serial.println("Fault detected! Turning pump OFF.");
    turnPumpOff();  // Ensure the motor is off in case of fault
    sendSMS("Line Fault Detected! Motor is OFF.", callerNumber); // Send to caller number
    Serial.println("Fault detected! SMS sent.");
  } else if (faultSignal == LOW && faultStatus) { // Fault cleared (Red Light OFF)
    faultStatus = false;
    Serial.println("Fault resolved. Sending notification.");
    sendSMS("Line Fault Resolved. System is back to normal.", callerNumber); // Send to caller number
    Serial.println("Fault resolved! SMS sent.");
  }
}

void extractCallerNumber(String response) {
  int startIndex = response.indexOf("+CLIP: \"") + 8;
  int endIndex = response.indexOf("\",", startIndex);
  if (startIndex > 0 && endIndex > 0) {
    callerNumber = response.substring(startIndex, endIndex);
    Serial.println("Caller Number Extracted: " + callerNumber);
  } else {
    Serial.println("Failed to extract caller number.");
  }
}

void autoAnswerCall() {
  Serial.println("Auto-answering call in 3 seconds...");
  delay(autoAnswerDelay); // Wait for the auto-answer delay
  Serial.println("Answering incoming call...");
  sendATCommand("ATA"); // Automatically answer the call
}

void handleDTMF(String response) {
  // Extract DTMF tone
  int toneIndex = response.indexOf("+DTMF: ");
  if (toneIndex != -1) {
    char tone = response.charAt(toneIndex + 7); // Extract tone character
    Serial.println("DTMF Tone Received: " + String(tone));

    switch (tone) {
      case '1':
        Serial.println("DTMF Tone 1: Turn Pump ON.");
        if (!faultStatus) {  // Only turn on pump if no fault
          turnPumpOn();
        } else {
          sendSMS("Cannot turn on motor. Line Fault Detected!", callerNumber);
        }
        break;
      case '2':
        Serial.println("DTMF Tone 2: Turn Pump OFF.");
        turnPumpOff();
        break;
      case '0':
        Serial.println("DTMF Tone 0: Sending Pump Status.");
        sendPumpStatus(); // Send both fault and motor status
        break;
      default:
        Serial.println("Invalid DTMF input received.");
    }
  } else {
    Serial.println("No valid DTMF tone detected.");
  }
}

void turnPumpOn() {
  if (faultStatus) {
    Serial.println("Cannot turn on pump due to line fault.");
    sendSMS("Cannot turn on motor. Line Fault Detected!", callerNumber);
    return;
  }
  digitalWrite(RELAY_PIN, HIGH); // Turn on relay
  pumpStatus = true;
  Serial.println("Pump turned ON.");
  sendSMS("Motor is now ON.", callerNumber);    // Send confirmation
}

void turnPumpOff() {
  digitalWrite(RELAY_PIN, LOW); // Turn off relay
  pumpStatus = false;
  Serial.println("Pump turned OFF.");
  sendSMS("Motor is now OFF.", callerNumber);   // Send confirmation
}

void sendPumpStatus() {
  String statusMessage;
  if (faultStatus) {
    statusMessage = "Line Fault Detected! Motor is OFF.";
  } else {
    statusMessage = pumpStatus ? "Motor is currently ON." : "Motor is currently OFF.";
  }
  Serial.println("Sending pump status: " + statusMessage);
  sendSMS(statusMessage, callerNumber);
}

void sendSMS(String message, String number) {
  if (number.length() > 0) {
    Serial.println("Sending SMS to: " + number);
    sim800.println("AT+CMGS=\"" + number + "\""); // Use caller's number
    delay(1000);
    sim800.println(message); // Send the message
    sim800.write(26);        // ASCII code for Ctrl+Z
    delay(2000);
    Serial.println("SMS Sent: " + message);
  } else {
    Serial.println("No valid caller number to send SMS.");
  }
}

void sendATCommand(String command) {
  sim800.println(command);
  delay(500);
  while (sim800.available()) {
    String response = sim800.readString();
    Serial.println("AT Command Response: " + response);
  }
}
