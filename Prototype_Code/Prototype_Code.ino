/*!
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

/*
 * Title: Prototype Code for UV Monitor
 * Authors: Connor Van Meter and Alex Hellenberg
 * EELE 498R - Capstone II
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
int array_count = 0; //index of arrays for both uv index and exposure
int index_notif_count = 0; //uv index notification counter so time between notifications can be changed
unsigned long save_time = 900000; //how long between array saves ms (900000 = 15 minutes)
char date_time[15];




/*if you want to see the save_time in action change the value to 60000 (1 minute)
 * and uncomment the block of 
 * serial prints at the bottom of the loop. You can watch through the serial monitor when it flips from storing in 
 * index and exposure array [0] and [1]
  */





void setup() {
  Serial.begin(115200);
  //Serial.println("VEML6075 Full Test");
  if (! uv.begin()) {
    Serial.println("Failed to communicate with VEML6075 sensor, check wiring?");
  }
  Serial.println("Found VEML6075 sensor");

  // Set the integration constant
  uv.setIntegrationTime(VEML6075_100MS);
  
  // Get the integration constant and print it!
  Serial.print("Integration time set to ");
  switch (uv.getIntegrationTime()) {
    case VEML6075_50MS: Serial.print("50"); break;
    case VEML6075_100MS: Serial.print("100"); break;
    case VEML6075_200MS: Serial.print("200"); break;
    case VEML6075_400MS: Serial.print("400"); break;
    case VEML6075_800MS: Serial.print("800"); break;
  }
  Serial.println("ms");
  
  // Set the high dynamic mode
  uv.setHighDynamic(true);
  // Get the mode
  /*
  if (uv.getHighDynamic()) {
    Serial.println("High dynamic reading mode");
  } else {
    Serial.println("Normal dynamic reading mode");
  }
  */
  // Set the mode
  uv.setForcedMode(false);
  // Get the mode
  /*
  if (uv.getForcedMode()) {
    Serial.println("Forced reading mode");
  } else {
    Serial.println("Continuous reading mode");
  }
  */
  // Set the calibration coefficients
  uv.setCoefficients(2.22, 1.33,  // UVA_A and UVA_B coefficients
                     2.95, 1.74,  // UVB_C and UVB_D coefficients
                     0.001461, 0.002591); // UVA and UVB responses
                     
}
/*---------------Loop--------------------//
//--------------------------------------*/
void loop() {
  rollingAverage();
  arraySave();
  
  //recieve data from iOS
  if(Serial.available()){
    String recieved = Serial.readString();
    if(recieved.indexOf("E")>=0){ //UV exposure limit recieved
      exposure_limit = recieved.toFloat();
      tone(A2,1000,80);
      delay(50);
      tone(A2,2000,80);
    }
    if(recieved.indexOf("I")>=0){ //UV index limit recieved
      index_limit = recieved.toFloat();
      tone(A2,1000,80);
      delay(50);
      tone(A2,2000,80);
    }
    if(recieved.indexOf("C")>=0){ //connected to bluetooth, recieve date_time in format "ddMMYYhhmmss"
        tone(A2,1047,80);
        delay(130);
        tone(A2,1319,80);
        delay(130);
        tone(A2,1568,80);
        delay(130);
        tone(A2,2093,80);
        delay(130);
        recieved.toCharArray(date_time,15); //This needs to be converted into date and time in an RTC library if possible
        sendExposure();
    }
    if(recieved.indexOf("D")>=0){ //stored data request
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
    /*Serial.print("In array0:");Serial.println(index_array[0],4);
    Serial.print("Ex array0:");Serial.println(exposure_array[0],4);
    Serial.print("In array1:");Serial.println(index_array[1],4);
    Serial.print("Ex array1:");Serial.println(exposure_array[1],4);
    Serial.print("Ex total:");Serial.println(exposure_total,4);
    Serial.print("Exposure:");Serial.println(current_exposure,4);
    Serial.print("Index:");Serial.println(current_index,4);*/
  delay(1000);
}

/*------------Exposure--------------------//
 * This function caluclates the 
 * total UV exposure.
//---------------------------------------*/
int getExposure() {
  current_exposure = index_array[array_count] * 22.5;
  return 0;
}

//Saves max values inside arrays and determines when the array should increment its index, this happens every amount of save_time which is currently 15 minutes
void arraySave(){
  static unsigned long lastSample = 0;
  unsigned long now = millis();
  if(array_count<=64){
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
  else if(now-lastSample>=save_time){
    lastSample += save_time;
    array_count++;
    current_index = 0;
    current_exposure = 0;
    }
  }
  else{
   
  }
}

//Sends exposure_total over Bluetooth
void sendExposure(){
  String string = String(exposure_total,DEC);
  String stringTwo = String(string + "E");
  char ex_send[16];
  stringTwo.toCharArray(ex_send,16);
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
  String ar_count = String(String(array_count)+"C");
  Serial.println(ar_count);
  char ar_count_send[4];
  ar_count.toCharArray(ar_count_send,4);
  Serial.write(ar_count_send);
  Serial.println();
  for(int i = 0;i<=array_count; i++){
    char i_send[5]; 
    String(index_array[i]).toCharArray(i_send,5);
    Serial.write(i_send);
    delayMicroseconds(500);
  }
}


/*------------rollingAverage--------------//
 * This function gets the rolling average of
 * the UV Index and stores it in an array of size 30.
 * 
 * NEED TO CONFIRM HOW OFTEN TO GET DATA
 * Every 5 sec? Can't remember.
 * 
//---------------------------------------*/
int rollingAverage() {
  //Rolling Average code here
  #define A_SIZE 15 //adjust volatility of index values: larger array = less volatility and vice versa
  double array[A_SIZE]; //array of size 30 to hold the average values
  static int index = 0; //variable to keep track of index in array
  static int t = 0; //variable to determine when we are measuring data
  static float sum; //total uv index measured
  static float average = 0; //average uv index
  static double newReading = 0; //variable to store a new uv index reading
  
  if(t == 0) { //if this is our first reading, we need to start at beginning of array
    newReading = uv.readUVI(); //take a new uvi reading
    array[index] = newReading; //the uvi reading is stored in the array
    sum += array[index]; //update the sum with the newly stored uvi reading
    if(index > 0) {
      average = sum / (index + 1);
    }
    t = 1;
  }
  else if(t != 0) {
    newReading = uv.readUVI();
    array[index] = newReading;
    if(index < A_SIZE-1) {
      sum = sum + array[index] - array[index + 1];
      average = sum/A_SIZE;
    }
    else{
      sum = sum + array[index] - array[0];
      average = sum/A_SIZE;
      index = 0;
    }
  }
  index++;
  current_index = average;
  sendIndex();
  return 0;
}

//-----------------------------------------------UNUSED FUNCTIONS-----------------------------------------//
/*---------------readSensor---------------//
 * This function reads raw data from the sensor.
 * getExposure function is not from the VEML sensor.
//---------------------------------------*/
/*int readSensor() {
  Serial.println();
  Serial.print("Raw UVA reading:  "); Serial.println(uv.readUVA());
  Serial.print("Raw UVB reading:  "); Serial.println(uv.readUVB());
  Serial.println("------------------------------------------------------");
  Serial.print("UV Index reading: "); Serial.println(uv.readUVI());
  Serial.print("UV Exposure reading: "); Serial.println(getExposure());
  Serial.println("------------------------------------------------------");
  return 0;
}*/
