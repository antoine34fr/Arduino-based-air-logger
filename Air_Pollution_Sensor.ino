#include "DHT.h" //sensor lib
#include <Wire.h> //lib to communocate with I2C/TWI devices
#include "SD.h" //logging lib
#include "RTClib.h" //clock lib
#include <SdFat.h> //lib needed to create a CSV file with a correct time/date 

RTC_PCF8523 rtc; // define the Real Time Clock object

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#define LOG_INTERVAL  3000 // mills between entries
#define SYNC_INTERVAL 3000 // mills between calls to flush() - to write data to the card
uint32_t syncTime = 0; // time of last sync()
#define DHTPIN 7     // what digital pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define        COV_RATIO                       0.2            //ug/mmm / mv
#define        NO_DUST_VOLTAGE                 400            //mv
#define        SYS_VOLTAGE                     5000           

DHT dht(DHTPIN, DHTTYPE); //initialize DHT sensor

const int chipSelect = 10; //data logging shield digital pin (also uses 11, 12, and 13)
const int iled = 5;                                            //drive the led of dust sensor
const int vout = 0;                                            //analog input of dust sensor

float density, voltage;
int   adcvalue;

/*
Filter function for the dust sensor :
*/
int Filter(int m)
{
  static int flag_first = 0, _buff[10], sum;
  const int _buff_max = 10;
  int i;
  
  if(flag_first == 0)
  {
    flag_first = 1;

    for(i = 0, sum = 0; i < _buff_max; i++)
    {
      _buff[i] = m;
      sum += _buff[i];
    }
    return m;
  }
  else
  {
    sum -= _buff[0];
    for(i = 0; i < (_buff_max - 1); i++)
    {
      _buff[i] = _buff[i + 1];
    }
    _buff[9] = m;
    sum += _buff[9];
    
    i = sum / 10.0;
    return i;
  }
}

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
  pinMode(iled, OUTPUT);
  digitalWrite(iled, LOW);                                     //iled default closed
  Serial.begin(9600);

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
    if (! SD.exists(filename)) {
      SdFile::dateTimeCallback(dateTime);  //properly time/date the new file
      logfile = SD.open(filename, FILE_WRITE);
      break;  // leave the loop!
    }
  }

  if (! logfile) {
    char moo1 [] = "Card failed, or not present" ;
    error(moo1);
  }

  Serial.print("Logging to: ");
  Serial.println(filename);


  dht.begin(); //initiate the sensor
  while (!Serial) {
    delay(1);  // for Leonardo/Micro/Zero
  }

  Wire.begin(); //initiate communication with the clock
  if (! rtc.begin()) {
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
  logfile.println("Time,Temperature (C),Humidity (%),Heat Index (C),Dust Density (µg/m3)");

}

void loop() {

  delay((LOG_INTERVAL - 1) - (millis() % LOG_INTERVAL));

  /*
  get adcvalue
  */
  digitalWrite(iled, HIGH);
  delayMicroseconds(280);
  adcvalue = analogRead(vout);
  digitalWrite(iled, LOW);
  
  adcvalue = Filter(adcvalue);
  
  /*
  covert voltage (mv)
  */
  voltage = (SYS_VOLTAGE / 1024.0) * adcvalue * 11;
  
  /*
  voltage to density
  */
  if(voltage >= NO_DUST_VOLTAGE)
  {
    voltage -= NO_DUST_VOLTAGE;
    
    density = voltage * COV_RATIO;
  }
  else
    density = 0;

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");

    return;
  }

  DateTime now = rtc.now();  //measure the time

  logfile.print(", ");  //DONT FORGET THE COMAS TO CHANGE CELLS IN THE CSV FILE
  logfile.print(now.hour(), DEC);
  logfile.print(':');
  logfile.print(now.minute(), DEC);
  logfile.print(':');
  logfile.print(now.second(), DEC);

  float hif = dht.computeHeatIndex(f, h);
  float hic = dht.computeHeatIndex(t, h, false);

  logfile.print(", ");
  logfile.print(t);
  logfile.print(", ");
  logfile.print(h);
  logfile.print(", ");
  logfile.print(hic);
  logfile.print(", ");
  logfile.print(density);
  logfile.println();

  if ((millis() - syncTime) < SYNC_INTERVAL) return; //NOT SURE WHAT THIS DOES
  syncTime = millis();

  logfile.flush();  //Actually writes in the file
  
  Serial.print(t);
  Serial.println(" degrees celcius");
  Serial.print(h);
  Serial.println(" % humidity");
  Serial.print(hic);
  Serial.println(" calculated heat index in deg celcius");
  Serial.print(density);
  Serial.print(" µg/m3 ");
  Serial.println("dust concentration");  
  Serial.print("Data written in file...");
  Serial.println();

}
