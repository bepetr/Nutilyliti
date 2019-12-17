#include <HardwareSerial.h>
#include <string.h>
#include <malloc.h>

// Create hardware serial object to communicate with SIM800L
#define SerialDataBits 115200
HardwareSerial simSerial(2); // SIM800L Tx & Rx is connected to ESP32 #16 & #17

int flipTheSwitch = 23; // output pin for relay control
int gateStatus = 0;
String inMessage;

/* Function prototypes */
void readMessage(String inMessage/*, int mode*/);
void checkAuthorization(String inMessage);
void sendSMS(String message, String phoneNr);
void updateSerial();

/*
 Setup function to be run once.
*/
void setup() {
  pinMode(flipTheSwitch, OUTPUT);
  // change?
  digitalWrite(flipTheSwitch, HIGH); // off by default -> gate is closed

  // Begin serial communication with Arduino IDE
  Serial.begin(9600);
  //delay(10000);
  Serial.setTimeout(3000); // read 3 seconds of data from serial at once

  // Begin serial communication with ESP32 and SIM800L
  simSerial.begin(SerialDataBits, SERIAL_8N1, 16, 17); // 17 = RX, 16 = TX
  
  Serial.println("Init...");
  delay(5000);

  simSerial.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  simSerial.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  simSerial.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();
  simSerial.println("AT+CREG?"); //Check whether it has registered in the network
  updateSerial();
  simSerial.println("AT+CPIN=4444"); // unlock the sim card
  updateSerial();
  simSerial.println("AT+CMGF=1"); // Configuring TEXT mode -> format is text
  updateSerial();
  simSerial.println("AT+CNMI=2,2,0,0,0"); // Send SMS data to serial output upon receipt
  updateSerial();
  simSerial.println("AT+CLIP=1"); // show caller information with incoming call
  updateSerial();
}

void readMessage(String inMessage/*, int mode*/) {
  //if (mode) {
  inMessage.toUpperCase();
  Serial.println(inMessage);
  // open or close gate
  if ((inMessage.indexOf("OPEN") >= 0) && !gateStatus) {
    // Flip the relay and save the current state
    digitalWrite(flipTheSwitch, LOW);
    gateStatus = 1; // gate is open
    Serial.println("Gate is open");
  }
  else if ((inMessage.indexOf("CLOSE") >= 0) && gateStatus) {
    // Flip the relay and save the current state
    digitalWrite(flipTheSwitch, HIGH);
    gateStatus = 0; // gate is closed
    Serial.println("Gate is closed");
  }
  else if (inMessage.indexOf("STATUS") >= 0) {
    // send SMS back with gate status
    String msg = "The gate is";
    if (gateStatus) msg = msg + "open";
    else msg = msg + "closed";
    Serial.println(msg);
    sendSMS(msg, "+37256623831"); // nt adminile
    Serial.println("Gate status request");
  }
  /*} else {
        if (gateStatus.indexOf("CLOSED")) {
                  digitalWrite(flipTheSwitch, LOW);
                  gateStatus = "OPEN"; // gate is open
                  Serial.println("Gate is open");
                } else if (gateStatus.indexOf("OPEN")) {
                  digitalWrite(flipTheSwitch, HIGH);
                  gateStatus = "CLOSED"; // gate is closed
                  Serial.println("Gate is closed");
                }
        }*/
  inMessage = "";
  delay(2000); // gate-dependent?
}

void checkAuthorization(String inMessage) {
  if (inMessage.indexOf("STATUS") >= 0) {
    // send SMS back with gate status
    gateStatus = 0;
    String msg = "The gate is";
    if (gateStatus) msg = msg + "open";
    else msg = msg + " closed";
    Serial.println(msg);
    sendSMS(msg, "+37256623831"); // nt adminile
    Serial.println("Gate status request");
  }
  char *buffer = (char*)malloc(sizeof(char) * 100);
  int i, r;
  String requestedState;
  int mode = -1; // 0 -> call, 1 -> sms
  strcpy(buffer, inMessage.c_str()); // convert input string to char*
  if (inMessage.indexOf("CMT") >= 0) { // SMS
      //Serial.println("sõnum");
      char* p = strstr(buffer, "+CMT:"); // write message starting with cmt from buffer to *p
      p = strtok(p, ","); // split p into tokens -> , is the delimiter
      if (p != NULL) {
        for (i = r = 0; p != NULL; i++, p = strtok(NULL, "\"")) { // 5 is the max nr of fields in the inMessage
          if (i == 1) { // extract phone book entry
            if (p == NULL) r = 0; // no entry in phonebook
            else r = p[0]; // first letter of contact name
          }
          else if (i == 4) { // extract requested state
            p = strtok(p, "\n");
            p = strtok(NULL, "\n");
            requestedState = p;
            break;
          }
        }
        if (r > 64) readMessage(requestedState/*, mode*/); // move on if authorization granted
        else Serial.println("Unauthorized text message!");
      }
    }
    else { // CALL
      //Serial.println("kõne");
      char* p = strstr(buffer, "+CLIP:"); // write message starting with clip from buffer to *p
      p = strtok(p, ",");
      if (p != NULL) {
        for (i = r = 0; p != NULL; i++, p = strtok(NULL, "\"")) {
          if (i == 4) { // 0 number, 1 type, 2 subaddress, 3 sub address type, 4 alphaId (contact name in phbook), 5 CLI Validity
            if (p == NULL) r = 0; // no entry in phonebook
            else r = p[0]; // first letter of contact name
            break;
          }
        }
        simSerial.println("ATH"); // hang up the call

        if (r > 64) { // if the contact was in the phonebook
          Serial.println("Known and authorized caller!");
          if (!gateStatus) {
            digitalWrite(flipTheSwitch, LOW);
            gateStatus = 1; // gate is open
            Serial.println("Gate is open");
          } else if (gateStatus) {
            digitalWrite(flipTheSwitch, HIGH);
            gateStatus = 0; // gate is closed
            Serial.println("Gate is closed");
          }
        } else {
          Serial.println("Unauthorized caller!");
        }
      }
    }
  }

void loop() {
  //updateSerial(); // to update the sim configuration via serial monitor
  if (simSerial.available()) {
    inMessage = simSerial.readString();
    checkAuthorization(inMessage);
  }
  delay(1000);
}

// Function to send SMS
void sendSMS(String message, String phoneNr) {
  // vaheta print-id write-ide vastu
  // send SMS to this number
  simSerial.println("AT+CMGS=\""+phoneNr+"\"");
  updateSerial();
  //delay(100);
  // send SMS
  //char* sendThis = (char*)malloc(sizeof(char)*10);
  //strcpy(sendThis, message.c_str()); // convert input string to char*
  simSerial.print(message);
  updateSerial();
  //delay(100);

  // End AT command
  simSerial.write(26); // 'Ctrl+z'
  delay(100);
 // simSerial.print();
  //updateSerial();
  // give module some time to send SMS
  delay(5000);
}


/*
void checkAuthorization(String inMessage) {
  char *buffer = (char*)malloc(sizeof(char) * 100);
  int i, r;
  String requestedState;
  int mode = -1; // 0 -> call, 1 -> sms
  strcpy(buffer, inMessage.c_str()); // convert input string to char*
  if (inMessage.indexOf("CMT") >= 0) { // SMS
      Serial.println("sõnum");
      mode = 1;
      char* p = strstr(buffer, "+CMT:"); // write message starting with cmt from buffer to *p
  else {
      Serial.println("kõne");
      mode = 0;
      char* p = strstr(buffer, "+CLIP:"); // write message starting with clip from buffer to *p
  }
  p = strtok(p, ","); // split p into tokens -> , is the delimiter
  if (p != NULL) {
      for (i = r = 0; p != NULL; i++, p = strtok(NULL, "\"")) {
          if (i == 1 && mode) { // extract phone book entry
            if (p == NULL) r = 0; // no entry in phonebook
            else r = p[0]; // first letter of contact name
          }
          else if (i == 4 && mode) { // extract requested state
            p = strtok(p, "\n");
            p = strtok(NULL, "\n");
            requestedState = p;
            break;
          }
          else if (i == 4 && !mode) { // 0 number, 1 type, 2 subaddress, 3 sub address type, 4 alphaId (contact name in phbook), 5 CLI Validity
            if (p == NULL) r = 0; // no entry in phonebook
            else r = p[0]; // first letter of contact name
            requestedState = "";
      simSerial.println("ATH"); // hang up the call
            break;
          }
        }
          
*/
/*
  Function for SIM module setup.
  Can be used for real-time configuration of the SIM card
  if enabled in loop().
  Note: this function should be disabled at normal operation.
*/
void updateSerial() {
  delay(500);
  while (Serial.available()) {
    simSerial.write(Serial.read()); // Forward what Serial received to Soft-Serial Port
  }
  while (simSerial.available()) {
    Serial.write(simSerial.read()); // Forward what Soft-Serial received to Serial Port
  }
}
