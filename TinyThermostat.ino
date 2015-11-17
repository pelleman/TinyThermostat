/**********************************************
 *
 * TinyThermostat
 *
 * Multi stage thermostat based on Atmel AtTiny.
 * 
 * Per-Ola Gustavsson
 * http://marsba.se
 * pelle@marsba.se
 *
 *
 *
 * Copyright 2015 Per-Ola Gustavsson
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
 *
 *********************************************/


// Arduino dependencies
#include <Arduino.h>
#include <EEPROM.h>

// Local dependencies
#include <temp.h>			// git@github.com:pelleman/temp.git
#include <blinkout.h>		// git@github.com:pelleman/blinkout.git


/**********************************************
 * Pinconfiguration
 *********************************************/

const int PIN_BUTTONS = 2;			// Analog button input
const int PIN_BUTTONS_DIGITAL = 4;	// Same pin as PIN_BUTTONS
									// Connect two buttons to ground via resistors
									// 180k for up and 100k for down
const int PIN_NTC = 3;				// Sensor input
									// Connect NTC to ground and a pullup resistor
									// to analog reference (VDD supply)
const int PIN_OUT_A = 0;			// Stage one output (set temp)
const int PIN_OUT_B = 2;			// Stage two output (set temp + HYSTERESIS)
const int PIN_OUT_C = 1;			// Stage three output (set temp + 2 HYSTERESIS)


/**********************************************
 * Internal mechanics
 *********************************************/

const int NTC_OHM25 = 95;			// NTC resistance at 25 degree C
const int PULLUP_OHM = 390;			// Pullup resistor at sensor input
const int NTC_B25_100 = 4330;		// B25/100 of NTC
const byte BUTTON_UP = 1;
const byte BUTTON_DOWN = 2;
const int BUTTON_DIVIDER = 107;		// Might need to be changed depending on
									// the internal pullup resistor

const float HYSTERESIS = 0.2;		// +- Hysteresis in Celcius
const float DELTA_TEMP = 2;			// Next stage above setTemp in Celcius;

const int STORAGE_HEADER = 4242;	// Check EEPROM for this header

float temp = 0;
int setTemp = 16;


/**********************************************
 *
 * Code
 *
 *
 *********************************************/

void setup() {
	pinMode(PIN_OUT_A, OUTPUT);
	pinMode(PIN_OUT_B, OUTPUT);
	pinMode(PIN_OUT_C, OUTPUT);
	pinMode(PIN_BUTTONS_DIGITAL, INPUT_PULLUP);
	analogReference(DEFAULT);
	loadData();
}

void loop() {
	
	// Read temperature
	temp = tempGetC(PIN_NTC, PULLUP_OHM, NTC_OHM25, NTC_B25_100);
	
	// Take action 
	if (temp > setTemp + HYSTERESIS)
		digitalWrite(PIN_OUT_A, HIGH);
	if (temp < setTemp - HYSTERESIS)
		digitalWrite(PIN_OUT_A, LOW);
	if (temp > setTemp + HYSTERESIS + DELTA_TEMP)
		digitalWrite(PIN_OUT_B, HIGH);
	if (temp < setTemp - HYSTERESIS + DELTA_TEMP)
		digitalWrite(PIN_OUT_B, LOW);
	if (temp > setTemp + HYSTERESIS + DELTA_TEMP + DELTA_TEMP)
		digitalWrite(PIN_OUT_C, HIGH);
	if (temp < setTemp - HYSTERESIS + DELTA_TEMP + DELTA_TEMP)
		digitalWrite(PIN_OUT_C, LOW);
	
	// Read input from buttons
	byte buttons = getButtons();
	
	// Wait to catch press of both buttons
	if (buttons != 0) {
		delay(50);
		buttons = getButtons();
	}
	switch (buttons) {
	case BUTTON_UP:
		blinkOut(PIN_OUT_C, int(temp + 0.5));
		delay(1000);
		break;
	case BUTTON_DOWN:
		blinkOut(PIN_OUT_C, setTemp);
		delay(1000);
		break;
		
	case BUTTON_UP | BUTTON_DOWN:	// Both buttons pressed
		digitalWrite(PIN_OUT_B, HIGH);
		digitalWrite(PIN_OUT_C, LOW);
		delay(3000);				// Wait to check long press
		buttons = (1023 - analogRead(PIN_BUTTONS)) / BUTTON_DIVIDER;
		if (buttons == BUTTON_UP | BUTTON_DOWN) {
			digitalWrite(PIN_OUT_C, HIGH);
			while (getButtons() != 0) {}
			digitalWrite(PIN_OUT_C, LOW);
			long lastPress = millis();
			long buttonUpTime = 0;
			long buttonDownTime = 0;
			long timeNow = 0;
			do {					// Loop until no button press for a few seconds
				timeNow = millis();
				buttons = getButtons();
				if ((buttons & BUTTON_UP) == BUTTON_UP) {
					if (timeNow > buttonUpTime + 200) {
						buttonUpTime = timeNow;
						lastPress = timeNow;
						setTemp++;
						digitalWrite(PIN_OUT_C, HIGH);
						delay(50);
						digitalWrite(PIN_OUT_C, LOW);
					}
				} else {
					buttonUpTime = 0;
				}
				if ((buttons & BUTTON_DOWN) == BUTTON_DOWN) {
					if (timeNow > buttonDownTime + 200) {
						buttonDownTime = timeNow;
						lastPress = timeNow;
						setTemp--;
						digitalWrite(PIN_OUT_B, LOW);
						delay(50);
						digitalWrite(PIN_OUT_B, HIGH);
					}
				} else {
					buttonDownTime = 0;
				}
			}
			while (timeNow < (lastPress + 3000));
			saveData();
			blinkOut(PIN_OUT_C, setTemp);
			delay(1000);
		}
		break;
		
	}
}

// Get the status of the buttons
byte getButtons()
{
	return (1023 - analogRead(PIN_BUTTONS)) / BUTTON_DIVIDER;
}

// Save to EEPROM
void loadData ()
{
	int header;
	EEPROM.get(1, header);

	if (header == STORAGE_HEADER) {
		EEPROM.get(3, setTemp);
	}
}
// Load from EEPROM
void saveData ()
{
	EEPROM.put(1, STORAGE_HEADER);
	EEPROM.put(3, setTemp);
}
