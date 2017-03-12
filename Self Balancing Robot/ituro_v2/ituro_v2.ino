const int sagileri = 5;
const int saggeri = 4;
const int solileri = 13;
const int solgeri = 10;
const int solenable = 11;
const int sagenable = 3;
float ex_pitch=0;
int pwm=0;

void ileri(int hiz){
analogWrite(sagenable, hiz); //220 degeri pwm olarak motor hızını ayarlıyor 255 e kadar çıkabilir analogWrite komutunu bunun icin kullaniyoruz
analogWrite(solenable, hiz);
digitalWrite(sagileri,LOW);
digitalWrite(saggeri, HIGH);
digitalWrite(solileri, LOW);
digitalWrite(solgeri, HIGH);
}

void sag()
{
analogWrite(solenable, 100);
analogWrite(sagenable, 220);
digitalWrite(sagileri, HIGH);
digitalWrite(saggeri, LOW);
digitalWrite(solileri, LOW);
digitalWrite(solgeri, HIGH);
}

void dur()
{
digitalWrite(sagileri, HIGH);
digitalWrite(saggeri, HIGH);
digitalWrite(solileri, HIGH);
digitalWrite(solgeri, HIGH);
}
void sol()
{
analogWrite(solenable, 220);
analogWrite(sagenable, 0);
digitalWrite(sagileri, LOW);
digitalWrite(saggeri, HIGH);
digitalWrite(solileri, HIGH);
digitalWrite(solgeri, LOW);
}


void geri(int hiz){
analogWrite(sagenable, hiz); //220 degeri pwm olarak motor hızını ayarlıyor 255 e kadar çıkabilir analogWrite komutunu bunun icin kullaniyoruz
analogWrite(solenable, hiz);
digitalWrite(sagileri,HIGH);
digitalWrite(saggeri, LOW);
digitalWrite(solileri, HIGH);
digitalWrite(solgeri, LOW);
}









/////////////////////////////////////////////I2C MPU6050////////////////////////////////////////////





#include <Wire.h>
#include <Kalman.h> // Source: https://github.com/TKJElectronics/KalmanFilter

#define RESTRICT_PITCH // Comment out to restrict roll to ±90deg instead - please read: http://www.freescale.com/files/sensors/doc/app_note/AN3461.pdf

Kalman kalmanX; // Create the Kalman instances
Kalman kalmanY;

/* IMU Data */
double accX, accY, accZ;
double gyroX, gyroY, gyroZ;
int16_t tempRaw;

double gyroXangle, gyroYangle; // Angle calculate using the gyro only
double compAngleX, compAngleY; // Calculated angle using a complementary filter
double kalAngleX, kalAngleY; // Calculated angle using a Kalman filter

uint32_t timer;
uint8_t i2cData[14]; // Buffer for I2C data

// TODO: Make calibration routine

const uint8_t IMUAddress = 0x68; // AD0 is logic low on the PCB
const uint16_t I2C_TIMEOUT = 1000; // Used to check for errors in I2C communication

uint8_t i2cWrite(uint8_t registerAddress, uint8_t data, bool sendStop) {
  return i2cWrite(registerAddress, &data, 1, sendStop); // Returns 0 on success
}

uint8_t i2cWrite(uint8_t registerAddress, uint8_t *data, uint8_t length, bool sendStop) {
  Wire.beginTransmission(IMUAddress);
  Wire.write(registerAddress);
  Wire.write(data, length);
  uint8_t rcode = Wire.endTransmission(sendStop); // Returns 0 on success
  if (rcode) {
    Serial.print(F("i2cWrite failed: "));
    Serial.println(rcode);
  }
  return rcode; // See: http://arduino.cc/en/Reference/WireEndTransmission
}

uint8_t i2cRead(uint8_t registerAddress, uint8_t *data, uint8_t nbytes) {
  uint32_t timeOutTimer;
  Wire.beginTransmission(IMUAddress);
  Wire.write(registerAddress);
  uint8_t rcode = Wire.endTransmission(false); // Don't release the bus
  if (rcode) {
    Serial.print(F("i2cRead failed: "));
    Serial.println(rcode);
    return rcode; // See: http://arduino.cc/en/Reference/WireEndTransmission
  }
  Wire.requestFrom(IMUAddress, nbytes, (uint8_t)true); // Send a repeated start and then release the bus after reading
  for (uint8_t i = 0; i < nbytes; i++) {
    if (Wire.available())
      data[i] = Wire.read();
    else {
      timeOutTimer = micros();
      while (((micros() - timeOutTimer) < I2C_TIMEOUT) && !Wire.available());
      if (Wire.available())
        data[i] = Wire.read();
      else {
        Serial.println(F("i2cRead timeout"));
        return 5; // This error value is not already taken by endTransmission
      }
    }
  }
  return 0; // Success
}


void setup() {

pinMode(sagileri,OUTPUT);
pinMode(saggeri,OUTPUT);
pinMode(solileri,OUTPUT);
pinMode(solgeri,OUTPUT);
pinMode(sagenable,OUTPUT);
pinMode(solenable,OUTPUT);
  ///////////////////////////////////////GYRO///////////////////////////////////

Serial.begin(115200);


  Wire.begin();
#if ARDUINO >= 157
  Wire.setClock(400000UL); // Set I2C frequency to 400kHz
#else
  TWBR = ((F_CPU / 400000UL) - 16) / 2; // Set I2C frequency to 400kHz
#endif

  i2cData[0] = 7; // Set the sample rate to 1000Hz - 8kHz/(7+1) = 1000Hz
  i2cData[1] = 0x00; // Disable FSYNC and set 260 Hz Acc filtering, 256 Hz Gyro filtering, 8 KHz sampling
  i2cData[2] = 0x00; // Set Gyro Full Scale Range to ±250deg/s
  i2cData[3] = 0x00; // Set Accelerometer Full Scale Range to ±2g
  while (i2cWrite(0x19, i2cData, 4, false)); // Write to all four registers at once
  while (i2cWrite(0x6B, 0x01, true)); // PLL with X axis gyroscope reference and disable sleep mode

  while (i2cRead(0x75, i2cData, 1));
  if (i2cData[0] != 0x68) { // Read "WHO_AM_I" register
    Serial.print(F("Error reading sensor"));
    while (1);
  }

  delay(100); // Wait for sensor to stabilize

  /* Set kalman and gyro starting angle */
  while (i2cRead(0x3B, i2cData, 6));
  accX = (i2cData[0] << 8) | i2cData[1];
  accY = (i2cData[2] << 8) | i2cData[3];
  accZ = (i2cData[4] << 8) | i2cData[5];

  // Source: http://www.freescale.com/files/sensors/doc/app_note/AN3461.pdf eq. 25 and eq. 26
  // atan2 outputs the value of -π to π (radians) - see http://en.wikipedia.org/wiki/Atan2
  // It is then converted from radians to degrees
#ifdef RESTRICT_PITCH // Eq. 25 and 26
  double roll  = atan2(accY, accZ) * RAD_TO_DEG;
  double pitch = atan(-accX / sqrt(accY * accY + accZ * accZ)) * RAD_TO_DEG;
#else // Eq. 28 and 29
  double roll  = atan(accY / sqrt(accX * accX + accZ * accZ)) * RAD_TO_DEG;
  double pitch = atan2(-accX, accZ) * RAD_TO_DEG;
#endif

  kalmanX.setAngle(roll); // Set starting angle
  kalmanY.setAngle(pitch);
  gyroXangle = roll;
  gyroYangle = pitch;
  compAngleX = roll;
  compAngleY = pitch;

  timer = micros();

}

void loop() {
  // put your main code here, to run repeatedly:

float pitch_acisi =  gyrooku();

//pitch_acisi -= 4;

  Serial.println(pitch_acisi);
 /* if ( pitch_acisi > 2) {
     pitch_acisi =  gyrooku();
     // pitch_acisi -= 4;

       if ( pitch_acisi > ex_pitch) {
        pwm= pwm+pitch_acisi-ex_pitch;
      geri(pwm*10);
    }

  }
   else if ( pitch_acisi < -2) {
     pitch_acisi =  gyrooku();
    //  pitch_acisi -= 4;
      if ( pitch_acisi < ex_pitch) {
        pwm= pwm-pitch_acisi+ex_pitch;
      ileri(pwm*10);
    }
  }
 */ 

  if ( pitch_acisi > 3) {
  while(1) {
 pitch_acisi =  gyrooku();
//7 pitch_acisi -= 4;
//  if(pitch_acisi > 5){
//    while(1) {
//        Serial.println(pitch_acisi);
//      pitch_acisi =  gyrooku();
//      ileri(150);
//      if(pitch_acisi < 3) {
//        break;
//      }
//    }
//  }
  geri(pitch_acisi*10);
  Serial.print(pitch_acisi);
  Serial.println("Geri donuyor!");
  if (pitch_acisi < 0 ) {
    Serial.println("Aci sifirlandi");
    dur();
    break;

  }
  if ( pitch_acisi > 60) {
    dur();
    break;
  }
  }
}

else if ( pitch_acisi < -3) {
  while(1) {
  pitch_acisi =  gyrooku();
//  pitch_acisi -= 4;
//  if(pitch_acisi < -5){
//    while(1) {
//        Serial.println(pitch_acisi);
//      pitch_acisi =  gyrooku();
//      geri(150);
//          if(pitch_acisi > -3) {
//        break;
//      }
//    }
 // }
  ileri(abs(pitch_acisi*10));
  Serial.println(pitch_acisi);
  Serial.println("Ileri gidiyor!");
  if (pitch_acisi > 0 ) {
    Serial.println("Aci sifirlandi");
    dur();
    break;
  }
  if ( pitch_acisi < -60) {
    dur();
    break;
  }
  }
}

}



float gyrooku() {
   /* Update all the values */
  while (i2cRead(0x3B, i2cData, 14));
  accX = ((i2cData[0] << 8) | i2cData[1]);
  accY = ((i2cData[2] << 8) | i2cData[3]);
  accZ = ((i2cData[4] << 8) | i2cData[5]);
  tempRaw = (i2cData[6] << 8) | i2cData[7];
  gyroX = (i2cData[8] << 8) | i2cData[9];
  gyroY = (i2cData[10] << 8) | i2cData[11];
  gyroZ = (i2cData[12] << 8) | i2cData[13];

  double dt = (double)(micros() - timer) / 1000000; // Calculate delta time
  timer = micros();

  // Source: http://www.freescale.com/files/sensors/doc/app_note/AN3461.pdf eq. 25 and eq. 26
  // atan2 outputs the value of -π to π (radians) - see http://en.wikipedia.org/wiki/Atan2
  // It is then converted from radians to degrees
#ifdef RESTRICT_PITCH // Eq. 25 and 26
  double roll  = atan2(accY, accZ) * RAD_TO_DEG;
  double pitch = atan(-accX / sqrt(accY * accY + accZ * accZ)) * RAD_TO_DEG;
#else // Eq. 28 and 29
  double roll  = atan(accY / sqrt(accX * accX + accZ * accZ)) * RAD_TO_DEG;
  double pitch = atan2(-accX, accZ) * RAD_TO_DEG;
#endif

  double gyroXrate = gyroX / 131.0; // Convert to deg/s
  double gyroYrate = gyroY / 131.0; // Convert to deg/s

#ifdef RESTRICT_PITCH
  // This fixes the transition problem when the accelerometer angle jumps between -180 and 180 degrees
  if ((roll < -90 && kalAngleX > 90) || (roll > 90 && kalAngleX < -90)) {
    kalmanX.setAngle(roll);
    compAngleX = roll;
    kalAngleX = roll;
    gyroXangle = roll;
  } else
    kalAngleX = kalmanX.getAngle(roll, gyroXrate, dt); // Calculate the angle using a Kalman filter

  if (abs(kalAngleX) > 90)
    gyroYrate = -gyroYrate; // Invert rate, so it fits the restriced accelerometer reading
  kalAngleY = kalmanY.getAngle(pitch, gyroYrate, dt);
#else
  // This fixes the transition problem when the accelerometer angle jumps between -180 and 180 degrees
  if ((pitch < -90 && kalAngleY > 90) || (pitch > 90 && kalAngleY < -90)) {
    kalmanY.setAngle(pitch);
    compAngleY = pitch;
    kalAngleY = pitch;
    gyroYangle = pitch;
  } else
    kalAngleY = kalmanY.getAngle(pitch, gyroYrate, dt); // Calculate the angle using a Kalman filter

  if (abs(kalAngleY) > 90)
    gyroXrate = -gyroXrate; // Invert rate, so it fits the restriced accelerometer reading
  kalAngleX = kalmanX.getAngle(roll, gyroXrate, dt); // Calculate the angle using a Kalman filter
#endif

  gyroXangle += gyroXrate * dt; // Calculate gyro angle without any filter
  gyroYangle += gyroYrate * dt;
  //gyroXangle += kalmanX.getRate() * dt; // Calculate gyro angle using the unbiased rate
  //gyroYangle += kalmanY.getRate() * dt;

  compAngleX = 0.93 * (compAngleX + gyroXrate * dt) + 0.07 * roll; // Calculate the angle using a Complimentary filter
  compAngleY = 0.93 * (compAngleY + gyroYrate * dt) + 0.07 * pitch;

  // Reset the gyro angle when it has drifted too much
  if (gyroXangle < -180 || gyroXangle > 180)
    gyroXangle = kalAngleX;
  if (gyroYangle < -180 || gyroYangle > 180)
    gyroYangle = kalAngleY;

  /* Print Data */
//#if 0 // Set to 1 to activate
//  Serial.print(accX); Serial.print("\t");
//  Serial.print(accY); Serial.print("\t");
//  Serial.print(accZ); Serial.print("\t");
//
//  Serial.print(gyroX); Serial.print("\t");
//  Serial.print(gyroY); Serial.print("\t");
//  Serial.print(gyroZ); Serial.print("\t");
//
//  Serial.print("\t");
//#endif

//  Serial.print(roll); Serial.print("\t");
//  Serial.print(gyroXangle); Serial.print("\t");
//  Serial.print(compAngleX); Serial.print("\t");
//  Serial.print(kalAngleX); Serial.print("\t");
//
//  Serial.print("\t");
//
//  Serial.print(pitch); Serial.print("\t");
//  Serial.print(gyroYangle); Serial.print("\t");
//  Serial.print(compAngleY); Serial.print("\t");
//  Serial.print(kalAngleY); Serial.print("\t");

//#if 0 // Set to 1 to print the temperature
//  Serial.print("\t");
//
//  double temperature = (double)tempRaw / 340.0 + 36.53;
//  Serial.print(temperature); Serial.print("\t");
//#endif
//
//  Serial.print("\r\n");

//  return  pitch;
return roll;
}
