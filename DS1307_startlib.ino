#include <Wire.h>

/*
Author: Jose Villalobos

This is an start-up library for facilitate the use of the DS1307 real time clock

Features:
1. Sets the time in 12h or 24h format (hh:mm:ss)
2. Sets the complete date (MM/DD/YY Day-Of-Week)
3. Gets the time in 12h or 24h format
4. Gets the complete date
5. Visualize the time in 12h or 24h format
6. Visualize the complete date
7. Safe DS1307 RAM reads
8. Safe DS1307 RAM write 
*/

// The DS1307 operates as a slave device in the data bus, It is identified by the address 1101000 (104)
#define CLOCK_ADDR 104

// Defining the RTC clock registers
#define SEC_ADDR 0x00
#define MIN_ADDR 0x01
#define HOUR_ADDR 0x02
#define DAY_ADDR 0x03
#define DATE_ADDR 0x04
#define MONTH_ADDR 0x05
#define YEAR_ADDR 0x06

// Mapping the ram limit registers
#define RTC_RAM_LOWER 0x08
#define RTC_RAM_UPPER 0x3F

// Wraps specified address to [RTC_RAM_LOWER, RTC_RAM_LOWER+56]
#define getSafeRamAddr(addr) (RTC_RAM_LOWER + (addr%57))

// Clamps a value between [min, max]
#define clamp(val, minVal, maxVal) ((val<minVal)?minVal:(val>maxVal)? maxVal: val)

// Define meridial values
typedef enum {
  AM, PM, NONE
} amOrPm;

// Defines days of week (dow)
typedef enum {
  MONDAY=1, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY
} Dow;

// Struct for pass and works with time
typedef struct {
  bool disableOsc; // Disable the oscillator of the DS1307
  byte second;
  byte minut;
  byte hour;
  bool is12Hour;
  amOrPm meridianIndicator;
} Time;

// Struc for work and pass the date
typedef struct {
  Dow dow; // [01, 07] day of week
  byte date; // [01, 31]
  byte month; // [01, 12]
  byte year; // [00, 99}
} Date;

static Date confDate;
static Date currentDate;

static Time confTime;
static Time currentTime;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  // Waits until the serial gets active
  while(!Serial)
  {}
  // Initializes the Wire library
  Wire.begin();
  
  // Configuring the time
  confTime.disableOsc = true;
  confTime.second = 58;
  confTime.minut = 37;
  confTime.hour = 20;
  confTime.meridianIndicator = AM;
  confTime.is12Hour = false;  
  // Send configuration variable to the DS1307
  configureTime(&confTime);

  // Configuring the date
  confDate.dow = MONDAY;
  confDate.date = 29;
  confDate.month = 10;
  confDate.year = 97;
  // Send to the DS1307 the configuration setup
  configureDate(&confDate);

  bool isSaved = saveInRTCRAM(5, 28);
  if (isSaved)
  {
    byte data = loadFromRTCRAM(5);
    Serial.println(data);
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  // Reads the time data from the DS1307
  readTime(&currentTime);
  // Visulaze the current time
  visualizeTime(&currentTime);

  // Reads the date data from the DS1307
  readDate(&currentDate);
  // Visualize the current date
  // in format MM/DD/YY DOW
  visualizeDate(&currentDate);
  
  delay(1000);
}

// Sends date configuration variable to the DS1307
void configureDate(Date* d)
{
  Wire.beginTransmission(CLOCK_ADDR);
  // The address of the Day-of-week  register
  Wire.write(DAY_ADDR);

  // Writes the value for the day of week register
  Wire.write(cvtDec2Bcd(clamp(d->dow, 1, 7)));

  // Writes the value for the date register
  Wire.write(cvtDec2Bcd(clamp(d->date, 1, 31)));

  // Writes the value for the month register
  Wire.write(cvtDec2Bcd(clamp(d->month, 1, 12)));

  // Writes the value for the year register
  Wire.write(cvtDec2Bcd(clamp(d->year, 0, 99)));

  if (Wire.endTransmission(true))
  {
    Serial.println("Date configured correctly");
  }
}

// Sends time configuration variable to the DS1307
void configureTime(Time* t)
{
  Wire.beginTransmission(CLOCK_ADDR);

  // Word address for the second's register
  Wire.write(SEC_ADDR);
  // Value for the second's registre
  // (0)001 0101 (The CH bit is set to 0 for enable the oscillator)
  // Sets the clock helt bit in the second's register
  byte ch = t->disableOsc << 7;

  // Convert number to binary encoded
  // Wraps seconds to [0, 59]
  Wire.write(ch | cvtDec2Bcd(clamp(t->second, 0, 59)));

  // Value for the minut's register
  // Wraps minuts to [0, 59]
  Wire.write(cvtDec2Bcd(clamp(t->minut, 0, 59)));

  byte hour = 0;
  // Value for the hour's register
  // If is configured in 12-hour mode
  if (t->is12Hour)
  {
    // When bit 7 is high the 12-hour mode is selected
    byte is12h = t->is12Hour << 6;
    // When bit 6 is high the PM is selected
    byte amPm = t->meridianIndicator << 5;
    // Clamps hour in [1, 12]
    byte times = cvtDec2Bcd(clamp(t->hour, 1, 12));
    // Compacts the values for the hour register
    hour = is12h | amPm | times;
  }
  else
  {
    byte is12h = t->is12Hour << 6;
    byte times = cvtDec2Bcd(clamp(t->hour, 0, 23));
    hour = is12h | times;
  }
  
  Wire.write(hour);
  if (Wire.endTransmission() == 0)
  {
    Serial.println("The hour was configured succesfully");  
  }
}

// Converts number from decimal to binary-coded decimal
byte cvtDec2Bcd(byte num)
{
  byte ones = num%10;
  byte tens = num/10;

  return (tens << 4) | ones;
}

void visualizeDate(Date* dateData)
{
  
  /*
   * Values that correspond to the day of week are user-defined but must be sequential (i.e., if 1 equals Sunday, then 2 equals
Monday, and so on.) Illogical time and date entries result in undefined operation.
  */
  Serial.print(dateData->month);
  Serial.print("/");
  Serial.print(dateData->date);
  Serial.print("/");
  Serial.print(dateData->year);
  switch(dateData->dow)
  {
    // Monday
    case MONDAY:
      Serial.print(" Monday");
      break;
    // Tuesday
    case TUESDAY:
      Serial.print(" Tuesday");
      break;
    // Wednesday
    case WEDNESDAY:
      Serial.print(" Wednesday");
      break;
    // Thursday
    case THURSDAY:
      Serial.print(" Thursday");
      break;
    // Friday
    case FRIDAY:
      Serial.print(" Friday");
      break;
    // Saturday
    case SATURDAY:
      Serial.print(" Saturday");
      break;
    // Sunday
    case SUNDAY:
      Serial.print(" Sunday");
      break;
  }
  Serial.println();
}

// Reads the actual date
// returns true if it succes, false if it does not succed
void readDate(Date* dateOut)
{
  // Addresses the DS1307
  Wire.beginTransmission(CLOCK_ADDR);
  Wire.write(DAY_ADDR);

  // Sends a restart message
  byte res = Wire.endTransmission(false);
  if (res == 0)
  {
    //return true;
  }
  else
  {
    return false;
  }

  // Request four bytes from the RTC containing the (Day, Date, Month, Year) and then send a STOP message for release the bus
  Wire.requestFrom(CLOCK_ADDR, 4, true);

  byte i = 0;
  // If there is a byte for read
  while(Wire.available() > 0)
  {
    /* IMPORTANT NOTE: The contents of the
time and calendar registers are in the BCD format

    https://en.wikipedia.org/wiki/Binary-coded_decimal
*/
    byte val = Wire.read();
    
    
    switch(i)
    {
      // Day of week
      case 0:
      {
        // dow == 0 does not have a flag associated
        Dow dow_ = MONDAY;        
        // Ensures no garbage with val & 0000 0111
        switch(val & 0x07)
        {
          // Monday
          case 1:
            dow_ = MONDAY;
            break;
          // Tuesday
          case 2:
            dow_ = TUESDAY;
            break;
          // Wednesday
          case 3:
            dow_ = WEDNESDAY;
            break;
          // Thursday
          case 4:
            dow_ = THURSDAY;
            break;
          // Friday
          case 5:
            dow_ = FRIDAY;
            break;
          // Saturday
          case 6:
            dow_ = SATURDAY;
            break;
          // Sunday
          case 7:
            dow_ = SUNDAY;
            break;
        }
        dateOut->dow = dow_;
      }
        break;
      // Date
      case 1:
      {
        byte units = val & 0x0F;
        byte tens = (val>>4) & 0x03;
        dateOut->date = tens*10 + units;
      }
        break;
      // Month
      case 2:
      {
        byte units = val & 0x0F;
        byte tens = (val>>4) & 0x01;
        dateOut->month = tens*10 + units;
      }
        break;
      case 3:
      {
        byte units = val & 0x0F;
        byte tens = (val>>4) & 0x0F;
        dateOut->year = tens*10 + units;
      }
        break;
    }
    
    i++;
  }
  
}

void visualizeTime(Time* timeData)
{
  Serial.print(timeData->hour);
  Serial.print(":");
  Serial.print(timeData->minut);
  Serial.print(":");
  Serial.print(timeData->second);

  if (timeData->is12Hour)
  {
    char* meridian = (timeData->meridianIndicator == PM) ? "PM" : "AM";
    Serial.print(meridian);
  }
  Serial.println();
}

// Reads the actual time and save it in the timeOut reference
void readTime(Time* timeOut)
{
  // Addresses the DS1307
  Wire.beginTransmission(CLOCK_ADDR);
  Wire.write(SEC_ADDR);
  // Is not needed to send the subsequent address because
  // in the DS1307:
  /*
   * Subsequent registers can be accessed sequentially until a STOP condition is
executed.
  */
  //Wire.write(MIN_ADDR);
  //Wire.write(HOUR_ADDR);

  // Sends a restart message
  byte res = Wire.endTransmission(false);
  if (res == 0)
  {
    // Serial.println("Preparing for receive the hour");
  }
  else
  {
    // Skip function if an error occured
    // Serial.println("An error while receiving the hour");
    return;
  }
  
  // Request three bytes from the RTC containing the (Seconds, Minuts, Hours) and then send a STOP message for release the bus
  Wire.requestFrom(CLOCK_ADDR, 3, true);
  byte i = 0;
  // If there is a byte for read
  while(Wire.available() > 0)
  {
    /* IMPORTANT NOTE: The contents of the
time and calendar registers are in the BCD format

    https://en.wikipedia.org/wiki/Binary-coded_decimal
*/
    byte val = Wire.read();

    // Proccess seconds and minuts
    if (i < 2)
    {      
      // val & 0011 1111 (0x3F)
      byte tens = (val >> 4) & 0x07;
      byte units = val & 0x0F;
      int res = tens * 10 + units;
      // If i==0 then program is reading the second's register
      if (i == 0)
      {
        // The bit 7 is the helt clock bit, (if it is high the oscillator is disabled)
        bool hc = (val>>4) & 0x08;
        // Saves the clock helt
        timeOut->disableOsc = hc;
        // Saves the seconds
        timeOut->second = res;
        // Serial.println(hc);
      }
      else
      {
        // Saves the minuts
        timeOut->minut = res; 
      } 
    }
    else if (i == 2)
    {
      /*Bit 6 of the hours register is defined as the 12-hour or
24-hour mode-select bit. When high, the 12-hour mode is selected. */
      // Checks if hour is 12 hour or not
      bool is12Hour = (val & (1 << 6)); // In c numbers different from 0 are considered true
      timeOut->is12Hour = is12Hour;
      if (is12Hour)
      {
        /*In the 12-hour mode, bit 5 is the AM/PM bit with
logic high being PM
        */
        bool isPM = val & (1 << 5);
        // Saves the meridian indicator if the hour is in 12h format
        timeOut->meridianIndicator = (isPM)?PM:AM;

        byte hour_ten = (val >> 4) & 0x01;
        byte hour_unit = val & 0x0F;
        byte hour = hour_ten*10 + hour_unit;
        timeOut->hour = hour;
      }
      else
      {
        timeOut->meridianIndicator = NONE;
        byte hour_ten = (val >> 4) & 0x03;
        byte hour_unit = val & 0x0F;
        byte hour = hour_ten*10 + hour_unit;
        timeOut->hour = hour;
      }
    }
    
    i++;
  }  
}

byte loadFromRTCRAM(byte initAddr)
{
  byte addr = getSafeRamAddr(initAddr);
  byte data = 0;

  Wire.beginTransmission(CLOCK_ADDR);
  Wire.write(addr);
  if (Wire.endTransmission(false) == 0)
  {
      // Serial.println("Prepraing for read data");
  }
  Wire.requestFrom(CLOCK_ADDR, 1, true);
  while (Wire.available() > 0)
  {
    data = Wire.read();
    return data;
  }  
}

// Returns true if the data was succesfully saved in specified address
bool saveInRTCRAM(byte initAddr, byte data)
{
  /*
   * NOTE: This implementation is inefficient when is used for send multiple
   * data because the data bus must be started, restarted and released for every
   * data.
  */
  
  // Addres the RTC device for write it
  Wire.beginTransmission(CLOCK_ADDR);
  // If initAddr is out of range put inside the range using the module function
  // This prevents the wrap perfomed by te DS1307 when ram address is upper RTC_RAM_UPPER
  // to change the RTC's registers
  // Wraps initAddr in [0, 56]
  byte addr = getSafeRamAddr(initAddr);
  // Writes the ram word address
  Wire.write(addr);
  // Writes a value to the last RAM word address
  Wire.write(data);
  if (Wire.endTransmission() == 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}
