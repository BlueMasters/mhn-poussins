/****************************************************************************
 * Poussins / Musée d'histoire naturelle de Fribourg.
/****************************************************************************
 * Copyright 2016 Jacques Supcik / Haute école d'ingénierie et d'architecture
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

struct uid {
    byte size;
    byte data[10];
};

#define RESET_PIN 49
#define DEBUG true

struct uid MASTERS[] = {
    {4, {0x84, 0x6f, 0x9d, 0xbb}},
};

struct sensor {
    int greenLED;
    int redLED;
    int cs;
    MFRC522 mfrc522;
    struct uid expectedUid;
};

struct sensor sensors[] = {
    // green  red  Chip
    // LED    LED  Select
    {  30,    40,  22},
    {  31,    41,  23},
    {  32,    42,  24},
    {  33,    43,  25},
    {  34,    44,  26},
    {  35,    45,  27},
    {  36,    46,  28},
};

const int N_OF_SENSORS = sizeof(sensors) / sizeof(sensor);
const int N_OF_MASTERS = sizeof(MASTERS) / sizeof(struct uid);

void setup() {
    Serial.begin(115200);
    SPI.begin();
    // Initialize all sensors
    for (int i = 0; i < N_OF_SENSORS; i++) {
        // Clear LEDs
        digitalWrite(sensors[i].greenLED, HIGH);
        digitalWrite(sensors[i].redLED, HIGH);
        pinMode(sensors[i].greenLED, OUTPUT);
        pinMode(sensors[i].redLED, OUTPUT);
        sensors[i].mfrc522.PCD_Init(sensors[i].cs, RESET_PIN);
    }
    // Load expected UIDs from EEPROM
    eepromLoadConfig();
}

void loop() {
    mainLoop:
    int okCount = 0;
    for (int i = 0; i < N_OF_SENSORS; i++) {
        struct sensor s = sensors[i];
        if (s.mfrc522.PICC_IsNewCardPresent() && s.mfrc522.PICC_ReadCardSerial()) {
            #ifdef DEBUG
            Serial.print("Seen sensor #");
            Serial.print(i);
            Serial.print(": ");
            dumpUid(s.mfrc522.uid.size, s.mfrc522.uid.uidByte);
            Serial.println();
            #endif
            if(isMaster(s.mfrc522.uid.size, s.mfrc522.uid.uidByte)){
                s.mfrc522.PICC_HaltA();
                s.mfrc522.PCD_StopCrypto1();
                learn();
                delay(1000);
                goto mainLoop; // I know that this is ugly, but it works
            } else if (areEqual(s.mfrc522.uid.size, s.mfrc522.uid.uidByte, &s.expectedUid)) {
                digitalWrite(s.redLED, HIGH);
                digitalWrite(s.greenLED, LOW);
                okCount++;
            } else {
                digitalWrite(s.greenLED, HIGH);
                digitalWrite(s.redLED, LOW);                
            }
        } else {
            digitalWrite(s.greenLED, HIGH);
            digitalWrite(s.redLED, HIGH);                
        }
    }
    if (okCount >= N_OF_SENSORS) {
        // All good!
    }
    delay(1000);
}

bool areEqual(byte size, byte uidByte[], struct uid* uid) {
    if (size != uid->size) return false;
    for (int i = 0; i < size; i++) {
        if (uidByte[i] != uid->data[i]) {
            return false; 
        }         
    }
    return true;
}

bool isMaster(byte size, byte uidByte[]) {
    for (int i = 0; i < N_OF_MASTERS; i++) {
        if(areEqual(size, uidByte, &MASTERS[i])){
            return true; 
        }
    }
    return false;
}
    
void dumpUid(byte size, byte uidByte[]) {
    for (byte i = 0; i < size-1; i++) {
        Serial.print(uidByte[i] < 0x10 ? "0" : "");
        Serial.print(uidByte[i], HEX);
        Serial.print(":");
    }
    if (int i = size-1 > 0) {
        Serial.print(uidByte[i] < 0x10 ? "0" : "");
        Serial.print(uidByte[i], HEX);
    }
}
