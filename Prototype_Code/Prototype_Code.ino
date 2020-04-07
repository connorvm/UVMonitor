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
 * Author: Connor Van Meter
 * EELE 498R - Capstone II
 * 
 */

 
#include <Wire.h>
#include "Adafruit_VEML6075.h"

Adafruit_VEML6075 uv = Adafruit_VEML6075();
float exposure_limit = 1000;
float index_limit = 1000;
void setup() {
  Serial.begin(115200);
  //Serial.println("VEML6075 Full Test");
  if (! uv.begin()) {
    //Serial.println("Failed to communicate with VEML6075 sensor, check wiring?");
  }
  //Serial.println("Found VEML6075 sensor");

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
  if (uv.getHighDynamic()) {
    Serial.println("High dynamic reading mode");
  } else {
    Serial.println("Normal dynamic reading mode");
  }

  // Set the mode
  uv.setForcedMode(false);
  // Get the mode
  if (uv.getForcedMode()) {
    Serial.println("Forced reading mode");
  } else {
    Serial.println("Continuous reading mode");
  }

  // Set the calibration coefficients
  uv.setCoefficients(2.22, 1.33,  // UVA_A and UVA_B coefficients
                     2.95, 1.74,  // UVB_C and UVB_D coefficients
                     0.001461, 0.002591); // UVA and UVB responses
}


/*---------------Loop--------------------//
//--------------------------------------*/
void loop() {
  //Serial.println();
  //readSensor();
  rollingAverage();
  if(Serial.available()){
    String recieved = Serial.readString();
    if(recieved.indexOf("E")>=0){
      exposure_limit = recieved.toFloat();
      tone(A2,1000,80);
      delay(50);
      tone(A2,2000,80);
    }
    if(recieved.indexOf("I")>=0){
      index_limit = recieved.toFloat();
      tone(A2,1000,80);
      delay(50);
      tone(A2,2000,80);
    }
  }

  delay(1000);
}


/*---------------readSensor---------------//
 * This function reads raw data from the sensor.
 * getExposure function is not from the VEML sensor.
//---------------------------------------*/
int readSensor() {
  Serial.println();
  Serial.print("Raw UVA reading:  "); Serial.println(uv.readUVA());
  Serial.print("Raw UVB reading:  "); Serial.println(uv.readUVB());
  Serial.println("------------------------------------------------------");
  Serial.print("UV Index reading: "); Serial.println(uv.readUVI());
  Serial.print("UV Exposure reading: "); Serial.println(getExposure());
  Serial.println("------------------------------------------------------");
  return 0;
}


/*------------Exposure--------------------//
 * This function caluclates the 
 * total UV exposure.
//---------------------------------------*/
int getExposure() {
  //Exposure calculations go here
  return 0;
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
  #define A_SIZE 30
  double array[A_SIZE]; //array of size 30 to hold the average values
  static int index = 0; //variable to keep track of index in array
  static int t = 0; //variable to determine when we are measuring data
  static float sum; //total uv index measured
  static float average = 0; //average uv index
  static double newReading = 0; //variable to store a new uv index reading
  
  if(t == 0) { //if this is our first reading, we need to start at beginning of array
    newReading = uv.readUVI(); //take a new uvi reading
    //Serial.print("t = 0 -- UVI = "); Serial.println(newReading);
    array[index] = newReading; //the uvi reading is stored in the array
    sum += array[index]; //update the sum with the newly stored uvi reading
    //Serial.print("array[index] = "); Serial.println(array[index]);
    //Serial.print("Sum (t = 0) = "); Serial.println(sum);
    if(index > 0) {
      average = sum / (index + 1);
    }
    t = 1;
  }
  else if(t != 0) {
    newReading = uv.readUVI();
    //Serial.print("t = 1 -- UVI = "); Serial.println(newReading);
    array[index] = newReading;
    if(index < 29) {
      sum = sum + array[index] - array[index + 1];
      //Serial.print("array[index] = "); Serial.println(array[index]);
      //Serial.print("array[index + 1] = "); Serial.println(array[index + 1]);
      //Serial.print("Sum (t = 1, index < 29) = "); Serial.println(sum);
      average = sum/30;
    }
    else{
      sum = sum + array[index] - array[0];
      //Serial.print("array[index] = "); Serial.println(array[index]);
      //Serial.print("array[0] = "); Serial.println(array[0]);
      //Serial.print("Sum (t = 1, index !< 29) = "); Serial.println(sum);
      average = sum/30;
      index = 0;
    }
  }
  index++;
  //Give an audible cue if the average index is over the set index limit
  if(average>=index_limit){
    tone(A2,2000,80);
    delay(130);
    tone(A2,2000,80);
    delay(130);
    tone(A2,2000,80);
    delay(130);
  }
  String stringOne = String(average, DEC);
  String uv = String(stringOne + "I");
  char uv_send[16];
  uv.toCharArray(uv_send,16);
  Serial.write(uv_send);
  delay(100);

  String string = String(13);
  String stringTwo = String(string + "E");
  char ex_send[16];
  stringTwo.toCharArray(ex_send,16);
  Serial.write(ex_send);
  delay(100);
    
  return 0;
}
