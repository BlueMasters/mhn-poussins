#include <Arduino.h>

/***************************************************************************
 * Poussins / Musée d'histoire naturelle de Fribourg
/***************************************************************************
 * Copyright 2016 Jacques Supcik <jacques.supcik@hefr.ch>
 *                Haute école d'ingénierie et d'architecture
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define DEBUG false
#define RESET_PIN 49
#define AUDIO_TRIGGER_PIN 7
#define REMOVE_TO_RETRIGGER 3

#define MILLISECOND 1
#define SECOND (1000 * MILLISECOND)
#define AUDIO_PULSE_WIDTH (100 * MILLISECOND)

struct uid {
    byte size;
    byte data[10];
};

// List of master keys
struct uid MASTERS[] = {
    {4, {0x84, 0x6f, 0x9d, 0xbb}},
    {4, {0x94, 0xb3, 0xa9, 0xbb}},
    {4, {0x64, 0x11, 0x29, 0xBB}},   
};

enum state {
    UNKNOWN,
    ABSENT, // There is no card in front of the sensor
    GOOD,   // The card in front of the sensor is the correct one
    WRONG,  // The card in front of the sensor is a bad one
};

struct sensor {
    int        redLED;
    int        greenLED;
    int        cs;
    MFRC522    mfrc522;
    enum state state;
    struct uid expectedUid;
};

// pin definition for the sensors
struct sensor sensors[] = {
    {.redLED = 30, .greenLED = 40, .cs = 22},
    {.redLED = 31, .greenLED = 41, .cs = 23},
    {.redLED = 32, .greenLED = 42, .cs = 24},
    {.redLED = 33, .greenLED = 43, .cs = 25},
    {.redLED = 34, .greenLED = 44, .cs = 26},
    {.redLED = 35, .greenLED = 45, .cs = 27},
    {.redLED = 36, .greenLED = 46, .cs = 28},
};

// pins definition for the status LED
const struct RGB {int red; int green; int blue;} statusLED = {11, 10, 12};

const int N_OF_SENSORS = sizeof(sensors) / sizeof(sensor);
const int N_OF_MASTERS = sizeof(MASTERS) / sizeof(struct uid);

unsigned long lastAudioTrigger = 0;

/***************************************************************************
 * Setup
 ***************************************************************************/

void setup() {
    Serial.begin(115200);
    SPI.begin();

    // Init LEDs and SPI sensors
    pinMode(statusLED.red,   OUTPUT);
    pinMode(statusLED.green, OUTPUT);
    pinMode(statusLED.blue,  OUTPUT);
    setStatus(0,0,0);

    digitalWrite(AUDIO_TRIGGER_PIN, HIGH);
    pinMode(AUDIO_TRIGGER_PIN, OUTPUT);
    digitalWrite(AUDIO_TRIGGER_PIN, LOW);

    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        s->state = UNKNOWN;
        ledOff(s);
        pinMode(s->greenLED, OUTPUT);
        pinMode(s->redLED,   OUTPUT);
    }

    // Make sure all chip select are HIGH
    // This is probably not needed, but it is safer
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        digitalWrite(s->cs, HIGH);
        pinMode(s->cs, OUTPUT);
    }

    // Init SPI sensors
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        s->mfrc522.PCD_Init(s->cs, RESET_PIN);
    }

    // Turn all LEDs green (visual control)
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        ledGreen(s);
    }
    setStatus(0,255,0);
    delay(2000);

    // Turn all LEDs red (visual control)
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        ledRed(s);
    }
    setStatus(255,0,0);
    delay(2000);

   // Turn all sensor LEDs OFF
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        ledOff(s);
    }

    // Turn status LED blue
    setStatus(0,0,255);

    int countOk = 0;
    // Check all sensors
    for (int i = 0; i < N_OF_SENSORS; i++) {
        #ifdef DEBUG
        Serial.print("Check sensor ");
        Serial.println(i);
        #endif
        struct sensor* s = &sensors[i];       
        
        #ifdef DEBUG
        s->mfrc522.PCD_DumpVersionToSerial();
        #endif
        bool result = s->mfrc522.PCD_PerformSelfTest();
        if (result) {
            #ifdef DEBUG
            Serial.println("Check passed");
            #endif
            countOk++;
            ledGreen(s);
        } else { // Result is false, test failed
            #ifdef DEBUG
            Serial.println("Check failed");
            #endif
            ledRed(s);
        }
        // Call PCD_Init once again. This is required after a self-test
        s->mfrc522.PCD_Init(s->cs, RESET_PIN);
        delay(100);
    }

    if (countOk != N_OF_SENSORS) {
        Serial.println("Sensors check failed. Abort.");
        for(;;) {/* forever */}
    }
    
    // Load expected UIDs from EEPROM
    eepromLoadConfig();
    delay(1000); /// wait 1 second

    // clear all LEDs
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        ledOff(s);
    }
    setStatus(0,0,0);
}

/***************************************************************************
 * Loop
 ***************************************************************************/

void loop() {
    int okCount = 0;
    unsigned long now = millis();

    static bool curOkState = false;
    static bool prevOkState = false;
    static bool audioPulse = false;
    static bool trigger = true;
    
    for (int i = 0; i < N_OF_SENSORS; i++) {
        #ifdef DEBUG
        // Serial.print("reading sensor ");
        // Serial.println(i);  
        #endif
        struct sensor* s = &sensors[i];
        
        if (s->mfrc522.PICC_IsNewCardPresent() &&
                s->mfrc522.PICC_ReadCardSerial()) {
            #ifdef DEBUG
            Serial.print("Card #");
            Serial.print(i);
            Serial.print(" = ");
            dumpUid(s->mfrc522.uid.size, s->mfrc522.uid.uidByte);
            Serial.println();
            #endif
            if (isMaster(s->mfrc522.uid.size, s->mfrc522.uid.uidByte)){
                s->mfrc522.PICC_HaltA();
                s->mfrc522.PCD_StopCrypto1();
                learn();
                delay(500);
                return;
            } else if (areEqual(
                    s->mfrc522.uid.size, s->mfrc522.uid.uidByte,
                    &s->expectedUid)) {
                if (s->state != GOOD) {
                    ledGreen(s);
                    s->state = GOOD;
                }
                okCount++;
            } else {
                if (s->state != WRONG) {
                    ledRed(s);
                    s->state = WRONG;
                }
            }
            // close session
            s->mfrc522.PCD_Init(s->cs, RESET_PIN);
        } else {
            if (s->state != ABSENT) {
                s->state = ABSENT;
                ledOff(s);
            }
        }
    }
 
    if (okCount <= AUDIO_TRIGGER_PIN - REMOVE_TO_RETRIGGER) {
        trigger = true;
    }
 
    // check of we have all sensors OK
    if (okCount >= N_OF_SENSORS) {
        setStatus(0,255,0);
        if (!prevOkState && trigger) {
            lastAudioTrigger = now;
            audioPulse = true;
            trigger = false;
            digitalWrite(AUDIO_TRIGGER_PIN, LOW);
            Serial.print("Cocorico ON");
        }
        prevOkState = curOkState;
        curOkState = true;
    } else {
        setStatus(255,0,0);
        prevOkState = curOkState;
        curOkState = false;
    }

    if (audioPulse && deltaT(now, lastAudioTrigger) > AUDIO_PULSE_WIDTH) {
        digitalWrite(AUDIO_TRIGGER_PIN, HIGH);
        Serial.print("Cocorico OFF");
        audioPulse = false;
    }
}

/***************************************************************************
 * Helpers
 ***************************************************************************/

//------------------------------------------
// computes t1 - t2 taking care of overflow 
//------------------------------------------
unsigned long deltaT(unsigned long t0, unsigned long t1) {
    if (t1 >= t0) { // normal case
         return t1 - t0;
    } else { // overflow of t1
        return t1 + (0xFFFFFFFFul - t0);
    }
}

//-------------------------------------------------
// Switch off the LED associated with the sensor s 
//-------------------------------------------------
void ledOff(struct sensor* s) {
    digitalWrite(s->greenLED, HIGH);
    digitalWrite(s->redLED,   HIGH);
}

//----------------------------------------------------
// Set the LED associated with the sensor s the green
//----------------------------------------------------
void ledGreen(struct sensor* s) {
    digitalWrite(s->redLED,   HIGH);
    digitalWrite(s->greenLED, LOW);
}

//--------------------------------------------------
// Set the LED associated with the sensor s the red
//--------------------------------------------------
void ledRed(struct sensor* s) {
    digitalWrite(s->greenLED, HIGH);
    digitalWrite(s->redLED,   LOW);
}

//-----------------------------------
// Set the status LED to a RGB value
//-----------------------------------
void setStatus(int r, int g, int b) {
    analogWrite(statusLED.red,   255-r);
    analogWrite(statusLED.green, 255-g);
    analogWrite(statusLED.blue,  255-b);
}

//-----------------------------
// Check if two UIDs are equal
//-----------------------------
bool areEqual(byte size, byte uidByte[], struct uid* uid) {
    if (size != uid->size) return false;
    for (int i = 0; i < size; i++) {
        if (uidByte[i] != uid->data[i]) return false;
    }
    return true;
}

//------------------------------------------------------------------
// Check if a card is a "master" card (a card used for programming)
//------------------------------------------------------------------
bool isMaster(byte size, byte uidByte[]) {
    for (int i = 0; i < N_OF_MASTERS; i++) {
        if(areEqual(size, uidByte, &MASTERS[i])){
            return true; 
        }
    }
    return false;
}

//-----------------------------------
// Dumps the UID to the serial port.
// Used for debugging
//-----------------------------------
void dumpUid(byte size, byte uidByte[]) {
    byte i;
    for (i = 0; i < size-1; i++) {
        Serial.print(uidByte[i] < 0x10 ? "0" : "");
        Serial.print(uidByte[i], HEX);
        Serial.print(":");
    }
    if ((i = size-1) > 0) {
        Serial.print(uidByte[i] < 0x10 ? "0" : "");
        Serial.print(uidByte[i], HEX);
    }
}

//-------------------------------------------------------------------------
// Set the system in learning mode and wait for all sensors to be assigned 
// the proper card
//-------------------------------------------------------------------------

bool learn() {
    boolean done[N_OF_SENSORS];
    int doneCount = 0;
    setStatus(0,0,255);
    
    // Initialize and switch all LEDs to red
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        done[i] = false;
        ledRed(s);
    }

    // Wait until all sensors are assigned
    while (doneCount < N_OF_SENSORS) {
        for (int i = 0; i < N_OF_SENSORS; i++) {
            struct sensor* s = &sensors[i];
            if (!done[i] &&
                    s->mfrc522.PICC_IsNewCardPresent() &&
                    s->mfrc522.PICC_ReadCardSerial()) {
                #ifdef DEBUG
                Serial.print("LEARNING sensor #");
                Serial.print(i);
                Serial.print(" = ");
                dumpUid(s->mfrc522.uid.size, s->mfrc522.uid.uidByte);
                Serial.println();
                #endif
                if (isMaster(s->mfrc522.uid.size, s->mfrc522.uid.uidByte)) {
                    #ifdef DEBUG
                    Serial.println("Ignoring master");
                    #endif
                    // Blink once
                    ledOff(s);
                    setStatus(255,165,0);  // Orange
                    delay(500);
                    ledRed(s);
                    setStatus(0,0,255);
                } else {
                    #ifdef DEBUG
                    Serial.println("Done");
                    #endif
                    ledGreen(s);
                    done[i] = true;
                    doneCount++;
                    s->expectedUid.size = s->mfrc522.uid.size;
                    memcpy(
                        s->expectedUid.data,
                        s->mfrc522.uid.uidByte,
                        s->mfrc522.uid.size);
                }
            }
        }
    }

    eepromSaveConfig();
    setStatus(0,255,0);
    delay(1000);

    // Clear all LEDs and reset state
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor* s = &sensors[i];
        ledOff(s);
        s->state = UNKNOWN;
    }

    setStatus(0,0,0);

}

/***************************************************************************
 * EEPROM management
 ***************************************************************************/

#define MAX_UID_SIZE      10
#define EEPROM_BLOCK_SIZE 16
// EEPROM_BLOCK_SIZE must be >= than MAX_UID_SIZE+1. A power of 2 makes the
// computation more efficient.

//----------------------------------------
// Write expected values (UIDs) to EEPROM
//-----------------------------------------
void eepromSaveConfig() {
    for (int i = 0; i < N_OF_SENSORS; i++) {
        int size = sensors[i].expectedUid.size;
        if (size > MAX_UID_SIZE) {
            size = MAX_UID_SIZE;
        }
        EEPROM.write(i*EEPROM_BLOCK_SIZE, size);
        for (int j = 0; j < size; j++) {
            // Don't forget to add 1 to the address (for the size)
            EEPROM.write(
                i*EEPROM_BLOCK_SIZE + j + 1,
                sensors[i].expectedUid.data[j]);
        }
    }
}

//-----------------------------------------
// Load expected values (UIDs) from EEPROM
//-----------------------------------------
void eepromLoadConfig() {
    for (int i = 0; i < N_OF_SENSORS; i++) {
        int size = EEPROM.read(i*EEPROM_BLOCK_SIZE);
        if (size > MAX_UID_SIZE) {
            size = MAX_UID_SIZE;
        }
        sensors[i].expectedUid.size = size;
        for (int j = 0; j < size; j++) {
            // Don't forget to add 1 to the address (for the size)
            sensors[i].expectedUid.data[j] = EEPROM.read(
                i*EEPROM_BLOCK_SIZE + j + 1);
        }
    }
}
