/*!
 * Title: Prototype Code for UV Monitor
 * Authors: Connor Van Meter and Alex Hellenberg
 * EELE 498R - Capstone II
 * 
 */

/*! Code for the VEML 6075 Sensor
 * @file veml6075_fulltest.ino
 *
 * A complete test of the library API with various settings available
 * 
 * Designed specifically to work with the VEML6075 sensor from Adafruit
 * ----> https://www.adafruit.com/products/3964
 *
 * These sensors use I2C to communicate, 2 pins (SCL+SDA) are required
 * to interface with the breakout.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * Written by Limor Fried/Ladyada for Adafruit Industries.  
 *
 * MIT license, all text here must be included in any redistribution.
 *
 */
 
#include <Wire.h>
#include <EEPROM.h>
#include "Adafruit_VEML6075.h"


Adafruit_VEML6075 uv = Adafruit_VEML6075();
float exposure_limit = 1000000; //limits set by user
float index_limit = 1000;
float current_index = 0; //the value of the current uv index
float current_exposure = 0; //current exposure max for the 15 minute time frame
float exposure_total = 0; //total daily exposure
float exposure_array[64]; //16 hours of exposure measurements every 15 min 
float index_array[64]; //MAX EEPROM SPACE IS 255 values
int array_count = 400; //index of arrays for both uv index and exposure
int index_notif_count = 0; //uv index notification counter so time between notifications can be changed
unsigned long save_time = 60000; //how long between arrayTimeCalc checks ms (60000 = 1 minute)
unsigned long timeConnected = 0;
unsigned long dayStart = 0;
unsigned long now = 0;
unsigned long date;

void setup() {
  Serial.begin(115200);
  //Serial.println("VEML6075 Full Test");
  if (! uv.begin()) {
    Serial.println("Failed to communicate with VEML6075 sensor, check wiring?");
  }
  //Serial.println("Found VEML6075 sensor");

  // Set the integration constant
  uv.setIntegrationTime(VEML6075_100MS);

  // Set the high dynamic mode
  uv.setHighDynamic(true);

  // Set the mode
  uv.setForcedMode(false);

  // Set the calibration coefficients
  uv.setCoefficients(2.22, 1.33,  // UVA_A and UVA_B coefficients
                     2.95, 1.74,  // UVB_C and UVB_D coefficients
                     0.001461, 0.002591); // UVA and UVB responses
                     
  for(int i = 0; i<64; i++){
    float Eindex;
    EEPROM.get(i*sizeof(float),Eindex);
    index_array[i] = Eindex;
    exposure_array[i] = index_array[i]*22.5;
    exposure_total += exposure_array[i];
  }
  EEPROM.get(400, index_limit);
  EEPROM.get(500, exposure_limit);
}
/*---------------Loop--------------------//
//--------------------------------------*/
void loop() {
  arraySave();
  
  //receive data from iOS
  if(Serial.available()){
    
    String received = Serial.readString();
    if(received.indexOf("E")>=0){ //UV exposure limit received
      exposure_limit = received.toFloat();
      tone(A2,1000,80);
      delay(50);
      tone(A2,2000,80);
      EEPROM.put(500, exposure_limit);
    }
    if(received.indexOf("I")>=0){ //UV index limit received
      index_limit = received.toFloat();
      tone(A2,1000,80);
      delay(50);
      tone(A2,2000,80);
      EEPROM.put(400, index_limit);
    }
    if(received.indexOf("C")>=0){ //connected to bluetooth, receive date_time in format "ddMMYYYY"
        tone(A2,1047,80);
        delay(130);
        tone(A2,1319,80);
        delay(130);
        tone(A2,1568,80);
        delay(130);
        tone(A2,2093,80);
        delay(130); 
        date = received.toInt();
        unsigned long Edate;
        EEPROM.get(300,Edate);
        if(Edate == 0){ //midnight rolled over and there could be values recorded from day in EEPROM so leave the index array alone and just replace date
          EEPROM.put(300,date);
        }
        else if(Edate != date){ //new date so clear all arrays and set new date in EEPROM
          for(int i = 0; i<64; i++){
            index_array[i] = 0;
            exposure_array[i] = 0;
            EEPROM.put(i*sizeof(float),index_array[i]);
            exposure_total = 0;
          }
          EEPROM.put(300,date);
        }
        //if dates are equal only time is updated
        sendExposure();
    }
    if(received.indexOf("T")>=0){ //time in seconds since start of day
      dayStart = received.toInt();
      timeConnected = millis();
      now = (millis() - timeConnected)/1000 + dayStart;
      arrayTimeCalc();
    }
    
    if(received.indexOf("D")>=0){ //stored data request from iOS
      sendData();
    }
    
  }
  
  //adjustable counter to decide how often alarm sounds for index notification
    if(current_index>index_limit){
      if(index_notif_count == 0){
        tone(A2,2000,80);
        delay(130);
        tone(A2,2000,80);
        delay(130);
        tone(A2,2000,80);
        delay(130);
        index_notif_count++;
      }
      else if(index_notif_count == 30){ //This number decides how often it is checked
      index_notif_count = 0;
      }
      else{
        index_notif_count++;
      }
    }
    else{
      index_notif_count = 0; //Once the current_index is below the limit the counter resets
    }
  delay(1000);
}

/*------------Exposure--------------------//
 * This function caluclates the 
 * total UV exposure.
//---------------------------------------*/
void getExposure() {
  if(index_array[array_count]>=0.01){
    current_exposure = index_array[array_count] * 22.5;
  }
}
/*--------------------------------------------------------------------*/

/*------------arraySave()----------------//
 * Saves max values inside arrays and 
 * determines when the array should increment 
 * its index. Checks current time every save_time
 * 
 //---------------------------------------*/
void arraySave(){
  
  unsigned long now_var = millis();

  static unsigned long lastSample = 0;
  
  if(array_count<64){
    rollingAverage(); //only get index values if we are within time frame
    if(current_index>index_array[array_count]){
    index_array[array_count] = current_index;//index_array[array_count] represents current maximum index for the 15 minute time period
    getExposure();  //exposure only calculated if new max index detected
    exposure_array[array_count] = current_exposure; //exposure_array[array_count] represents total exposure during 15 minute time period based on index max for that period
    exposure_total = 0;
    for (int i = 0; i<=array_count; i++){
      exposure_total += exposure_array[i];
    }
    sendExposure();//exposure only sent when 15 minute max is detected
    if(exposure_total>= exposure_limit){
        tone(A2,1047,80);
        delay(130);
        tone(A2,2093,80);
        delay(130);
        tone(A2,1047,80);
        delay(130);
    }
  }
  
  if(now_var-lastSample>=save_time){ //every minute check time, update now, and store in EEPROM within 5:30-9:30 EEPROM use if 6666 days with this storage method at minimum
    lastSample += save_time;
    now += save_time/1000;
    arrayTimeCalc();
    EEPROM.put(array_count*sizeof(float),index_array[array_count]);
    }
  }
  //if time is outside 5:30AM - 9:30PM stop saving to array
  //these statements allow device to continue recording data correctly if power is not lost over multiple days
  else if(array_count == 64){
    now = (millis()-timeConnected)/1000 + dayStart;
    if (now >= 86400){ //if midnight is reached set now to 0 and clear arrays
      now = 0;
      lastSample = millis();
      timeConnected = millis(); //simulate connection to iOS by setting timeConnected to current time and dayStart to 0
      dayStart = 0;
      array_count++;
      EEPROM.put(300,(unsigned long)0);
      for(int i = 0;i<=array_count; i++){
        index_array[i] = 0;
        exposure_array[i] = 0;
        EEPROM.put(i*sizeof(float),index_array[i]);
      }
    }
  }
  else if(array_count == 65 && now_var-lastSample >= 19800000){ //midnight has been reached look for 5:30AM 
    lastSample = millis();
    now = 19800;
    array_count = 0;
  }
}

//Sends exposure_total over Bluetooth

/*--------------------------------------------------------------------*/
void sendExposure(){
  String string = String(exposure_total,DEC);
  String stringTwo = String(string + "E");
  char ex_send[17];
  stringTwo.toCharArray(ex_send,17);
  Serial.write(ex_send);
  delay(100);
}

void sendIndex(){
  String stringOne = String(current_index,DEC);
  String uv = String(stringOne + "I");
  char uv_send[16];
  uv.toCharArray(uv_send,16);
  Serial.write(uv_send);
  delay(100);
}


void sendData(){
  for(int i = 0;i<64; i++){
    char i_send[4]; 
    String(index_array[i]).toCharArray(i_send,4);
    i_send[3] = 'D';
    Serial.write(i_send,4);
    delay(50);
  }
}



/*------------rollingAverage--------------//
 * This function gets the rolling average of
 * the UV Index and stores it in an array of size 30.
 * 
 * 
 *
 * 
//---------------------------------------*/
void rollingAverage() {
  //Rolling Average code here
  #define A_SIZE 30 //adjust volatility of index values: larger array = less volatility and vice versa
  static double array[A_SIZE]; //array of size 30 to hold the average values
  static int index = 0; //variable to keep track of index in array
  static int t = 0; //variable to determine when we are measuring data
  static float sum; //total uv index measured
  static float average = 0; //average uv index
  static double newReading = 0; //variable to store a new uv index reading


  if(t == 0) { //if this is our first reading, we need to start at beginning of array
    newReading = uv.readUVI(); //take a new uvi reading
    array[index] = newReading; //the uvi reading is stored in the array
    sum += array[index]; //update the sum with the newly stored uvi reading
    average = sum / (index + 1);
    index++;
    if(index ==A_SIZE){
        t = 1;
        index = 0;
    }     
  }
  else if(t != 0) {
    newReading = uv.readUVI();
    sum -=array[index];
    array[index] = newReading;
    sum +=array[index];
    average = sum/A_SIZE;
    index++;
    if(index == A_SIZE){
      index = 0;
    }
  }
  current_index = average;
  sendIndex();
}
/*--------------------------------------------------------------------*/

void arrayTimeCalc(){
  if(now >= 47700){
    if(now >= 62100){
      if(now >= 69300){
        if(now >= 72900){
          if(now >= 74700){
            if(now >= 75600){
              if(now >= 76500){
                if(now >=77400){
                  array_count = 64;
                }
                else{
                  array_count = 63;
                }
              }
              else{
                array_count = 62;
              }
            }
            else{
              array_count = 61;
            }
          }
          else{
            if(now >= 73800){
              array_count = 60;
            }
            else{
              array_count = 59;
            }
          }
        }
        else{
          if(now >= 71100){
            if(now >= 72000){
              array_count = 58;
            }
            else{
              array_count = 57;
            }
          }
          else{
            if(now >= 70200){
              array_count = 56;
            }
            else{
              array_count = 55;
            }
          }
        }
      }
      else{ //48-56
        if(now >= 65700){
          if(now >= 67500){ 
            if(now >= 68400){
              array_count = 54;
            }
            else{
              array_count = 53;
            }
          }
          else{//52-54
            if(now >= 66600){
              array_count = 52;
            }
            else{
              array_count = 51;
            }
          }
        }
        else{
          if(now >= 63900){
            if(now >= 64800){
              array_count = 50;
            }
            else{
              array_count = 49;
            }
          }
          else{
            if(now >= 63000){
              array_count = 48;
            }
            else{
              array_count = 47;
            }
          }
        }
      }
    }
    else{ //32-48
      if(now >= 54900){
        if(now >= 58500){
          if(now >= 60300){
            if(now >= 61200){
              array_count = 46;
            }
            else{
              array_count = 45;
            }
          }
          else{
            if(now >= 59400){
              array_count = 44;
            }
            else{
              array_count = 43;
            }
          }
        }
        else{
          if (now >= 56700){
            if(now >= 57600){
              array_count = 42;
            }
            else{
              array_count = 41;
            }
          }
          else{
            if(now >= 55800){
              array_count = 40;
            }
            else{
              array_count = 39;
            }
          }
        }
      }
      else{ //32-40
      if(now >= 51300){
        if(now >= 53100){
          if(now >= 54000){
            array_count = 38;
          }
          else{
            array_count = 37;
          }
        }
        else{
          if(now >= 52200){
            array_count = 36;
          }
          else{
            array_count = 35;
          }
        }
      }
      else{ //32-36
        if(now >= 49500){
          if(now >= 50400){
            array_count = 34;
          }
          else{
            array_count = 33;
          }
        }
        else{ //32-34
          if(now >= 48600){
            array_count = 32;
          }
          else{
            array_count = 31;
          }
        }
      }
    }
  }
 }
  else{ //<32
    if(now >= 33300){
      if(now >= 40500){
        if(now >= 44100){
          if(now >= 45900){
            if(now >= 46800){
              array_count = 30;
            }
            else{
              array_count = 29;
            }
          }
          else{
            if(now >= 45000){
              array_count = 28;
            }
            else{
              array_count = 27;
            }
          }
        }
        else{
          if(now >= 42300){
            if(now >= 43200){
              array_count = 26;
            }
            else{
              array_count = 25;
            }
          }
          else{
            if(now >= 41400){
              array_count = 24;
            }
            else{
              array_count = 23;
            }
          }
        }
      }
      else{
        if(now >= 36900){
          if(now >= 38700){
            if(now >= 39600){
              array_count = 22;
            }
            else{
              array_count = 21;
            }
          }
          else{
            if(now >= 37800){
              array_count = 20;
            }
            else{
              array_count = 19;
            }
          }
        }
        else{
          if(now >= 35100){
            if(now >= 36000){
              array_count = 18;
            }
            else{
              array_count = 17;
            }
          }
          else{
            if(now >= 34200){
              array_count = 16;
            }
            else{
              array_count = 15;
            }
          }
        }
      }
    }
    else{
      if(now >= 26100){
        if(now >= 29700){
          if(now >= 31500){
            if(now >= 32400){
              array_count = 14;
            }
            else{
              array_count = 13;
            }
          }
          else{
            if(now >= 30600){
              array_count = 12;
            }
            else{
              array_count = 11;
            }
          }
        }
        else{
          if(now >= 27900){
            if(now >= 28800){
              array_count = 10;
            }
            else{
              array_count = 9;
            }
          }
          else{
            if(now >= 27000){
              array_count = 8;
            }
            else{
              array_count = 7;
            }
          }
        }
      }
      else{
        if(now >= 22500){
          if(now >= 24300){
            if(now >= 25200){
              array_count = 6;
            }
            else{
              array_count = 5;
            }
          }
          else{
            if(now >= 23400){
              array_count = 4;
            }
            else{
              array_count = 3;
            }
          }
        }
        else{
          if(now >= 20700){
            if(now >= 21600){
              array_count = 2;
            }
            else{
              array_count = 1;
            }
          }
          else{
            if(now >= 19800){
              array_count = 0;
            }
            else{
              //No recording
            }
          }
        }
      }
    }
  }
}
