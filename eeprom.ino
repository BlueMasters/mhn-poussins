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

#define EEPROM_BLOCK_SIZE 16

void eepromSaveConfig() {
    for (int i = 0; i < N_OF_SENSORS; i++) {
        int size = sensors[i].expectedUid.size;
        EEPROM.write(i*EEPROM_BLOCK_SIZE, size);
        for (int j = 0; j < size; j++) {
            EEPROM.write(i*EEPROM_BLOCK_SIZE+j+1, sensors[i].expectedUid.data[j]);
        }
    }
}

void eepromLoadConfig() {
    for (int i = 0; i < N_OF_SENSORS; i++) {
        int size = EEPROM.read(i*EEPROM_BLOCK_SIZE);
        sensors[i].expectedUid.size = size;
        for (int j = 0; j < size; j++) {
            sensors[i].expectedUid.data[j] = EEPROM.read(i*EEPROM_BLOCK_SIZE+j+1);
        }
    }
}
