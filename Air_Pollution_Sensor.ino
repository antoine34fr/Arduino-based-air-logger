#include <LiquidCrystal.h> //For some reason, the libraries order matters!!
#include <Wire.h>
#include "SD.h"
#include "RTClib.h"
#include <SdFat.h>

int measurePin = 5;
int ledPower = 12;

int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

RTC_PCF8523 rtc; // define the Real Time Clock object

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

LiquidCrystal lcd(9, 8, 5, 4, 3, 2); //LCD display pins - don't use 10, 11, 12, or 13: used by data logger

#define LOG_INTERVAL  1000 // mills between entries
#define SYNC_INTERVAL 1000 // mills between calls to flush() - to write data to the card
uint32_t syncTime = 0; // time of last sync()

const int chipSelect = 10; //data logging shield digital pin (also uses 11, 12, and 13)

File logfile; //the logging file

void error(char *str) //error function if problem with SD card
{
	Serial.print("error: ");
	Serial.println(str);

	while (1);
}

void dateTime(uint16_t* date, uint16_t* time) { //function to create an accurately timed/dated file
	DateTime now = rtc.now();

	// return date using FAT_DATE macro to format fields
	*date = FAT_DATE(now.year(), now.month(), now.day());

	// return time using FAT_TIME macro to format fields
	*time = FAT_TIME(now.hour(), now.minute(), now.second());
}

void setup()
{
	Serial.begin(9600);

	pinMode(ledPower, OUTPUT);

	lcd.begin(16, 2); //number of columns and rows of the lcd display

	Serial.print("Initializing SD card...");
	pinMode(10, OUTPUT);

	if (!SD.begin(chipSelect)) {
		Serial.println("Card failed, or not present");
		return;
	}
	Serial.println("card initialized.");

	char filename[] = "LOGGER00.CSV"; //creates a file
	for (uint8_t i = 0; i < 100; i++) {
		filename[6] = i / 10 + '0';
		filename[7] = i % 10 + '0';
		if (!SD.exists(filename)) {
			SdFile::dateTimeCallback(dateTime);  //properly time/date the new file
			logfile = SD.open(filename, FILE_WRITE);
			break;  // leave the loop!
		}
	}

	if (!logfile) {
		char moo1[] = "Card failed, or not present";
		error(moo1);
	}

	Serial.print("Logging to: ");
	Serial.println(filename);

	Wire.begin(); //initiate communication with the clock
	if (!rtc.begin()) {
		Serial.println("RTC failed");
	}

	DateTime now = rtc.now();  //measure the time and date

	logfile.print(now.year(), DEC);  //print the following in the new file. once.
	logfile.print('/');
	logfile.print(now.month(), DEC);
	logfile.print('/');
	logfile.print(now.day(), DEC);
	logfile.print(" - ");
	logfile.print(daysOfTheWeek[now.dayOfTheWeek()]);
	logfile.print(", ");
	logfile.println("Raw Value,Voltage,Particle Concentration (µg/m3)");

}

void loop()
{
	delay((LOG_INTERVAL - 1) - (millis() % LOG_INTERVAL));

	digitalWrite(ledPower, LOW); // power on the LED
	delayMicroseconds(samplingTime);

	voMeasured = analogRead(measurePin); // read the dust value

	delayMicroseconds(deltaTime);
	digitalWrite(ledPower, HIGH); // turn the LED off
	delayMicroseconds(sleepTime);

	// 0 - 5.0V mapped to 0 - 1023 integer values 
	calcVoltage = voMeasured * (5.0 / 1024);

	// linear eqaution taken from http://www.howmuchsnow.com/arduino/airquality/
	// Chris Nafis (c) 2012
	dustDensity = (0.17 * calcVoltage - 0.1) * 1000;

	Serial.print("Raw Signal Value (0-1023): ");
	Serial.print(voMeasured);

	Serial.print(" - Voltage: ");
	Serial.print(calcVoltage);

	Serial.print(" - Dust Density [ug/m3]: ");
	Serial.println(dustDensity);

	lcd.print("[C]=");  //display the following info on the LCD
	lcd.setCursor(2, 0);
	lcd.print(dustDensity);
	lcd.print(" µg/m3");

	if (isnan(measurePin)) {
		Serial.println("Failed to read from sensor!");

		return;
	}

	DateTime now = rtc.now();  //measure the time

	logfile.print(", ");  //DONT FORGET THE COMAS TO CHANGE CELLS IN THE CSV FILE
	logfile.print(now.hour(), DEC);
	logfile.print(':');
	logfile.print(now.minute(), DEC);
	logfile.print(':');
	logfile.print(now.second(), DEC);

	logfile.print(", ");
	logfile.print(voMeasured);
	logfile.print(", ");
	logfile.print(calcVoltage);
	logfile.print(", ");
	logfile.print(dustDensity);

	logfile.println();

	if ((millis() - syncTime) < SYNC_INTERVAL) return; //NOT SURE WHAT THIS DOES
	syncTime = millis();

	logfile.flush();  //Actually writes in the file

	Serial.println("Data written in file...");

}
