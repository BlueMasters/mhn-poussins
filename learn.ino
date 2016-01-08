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

bool learn() {
    boolean done[N_OF_SENSORS];
    int doneCount = 0;
    // Initialize and switch all LEDs to red
    for (int i = 0; i < N_OF_SENSORS; i++) {
        done[i] = false;
        digitalWrite(sensors[i].greenLED, HIGH);
        digitalWrite(sensors[i].redLED, LOW);
    }

    while (doneCount < N_OF_SENSORS) {
        for (int i = 0; i < N_OF_SENSORS; i++) {
            struct sensor s = sensors[i];
            if (!done[i] && s.mfrc522.PICC_IsNewCardPresent() && s.mfrc522.PICC_ReadCardSerial()) {
                #ifdef DEBUG
                Serial.print("LEARNING sensor #");
                Serial.print(i);
                Serial.print(": ");
                dumpUid(s.mfrc522.uid.size, s.mfrc522.uid.uidByte);
                Serial.println();
                #endif
                // Switch LED to green
                digitalWrite(s.redLED, HIGH);
                digitalWrite(s.greenLED, LOW);
                done[i] = true;
                doneCount++;
                s.expectedUid.size = s.mfrc522.uid.size;
                memcpy(s.expectedUid.data, s.mfrc522.uid.uidByte, s.mfrc522.uid.size);
            }
        }
    }

    eepromSaveConfig();

    // Clear all LEDs
    for (int i = 0; i < N_OF_SENSORS; i++) {
        digitalWrite(sensors[i].greenLED, HIGH);
        digitalWrite(sensors[i].redLED, HIGH);
    }

}