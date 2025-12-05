#include <TheThingsNetwork.h>

int sensorPin = A0;   // select the input pin for the potentiometer
int sensorValue = 0;  // variable to store the value coming from the sensor

const char *appEui = "0004A30B001E5DD8";
const char *appKey = "DE34455E0B747CA641D9D34CFA19040C";

// Replace REPLACE_ME with TTN_FP_EU868 or TTN_FP_US915
#define freqPlan TTN_FP_EU868

#define loraSerial Serial1
#define debugSerial Serial

#define maxHeartRate 300;

// ECG monitoring variables
long instance1=0, timer;
double hrv =0, hr = 72, interval = 0;
int count = 0;  
bool flag = 0;
#define threshold 300 // to identify R peak (adjusted for 0-1023 range)
#define timer_value 10000 // 10 seconds timer to calculate hr

// Abnormal value detection thresholds
#define MIN_NORMAL_HR 40    // Minimum normal heart rate (bradycardia below this)
#define MAX_NORMAL_HR 180   // Maximum normal heart rate (tachycardia above this)
#define MAX_NORMAL_HRV 100  // Maximum reasonable HRV value
#define MIN_NORMAL_HRV -100 // Minimum reasonable HRV value
#define MIN_SENSOR_VALUE 0  // Minimum sensor reading
#define MAX_SENSOR_VALUE 1023 // Maximum sensor reading

// The time to wait after each loop
#define dataRate 10
#define transmissionRate 30000 // Send data every 30 seconds

unsigned long lastTransmission = 0;

TheThingsNetwork ttn(loraSerial, debugSerial, freqPlan);

void setup() {
  loraSerial.begin(57600);
  debugSerial.begin(9600);

  // Wait a maximum of 10s for Serial Monitor
  while (!debugSerial && millis() < 10000)
    ;

  debugSerial.println("-- STATUS");
  ttn.showStatus();

  debugSerial.println("-- JOIN");
  ttn.join(appEui, appKey);
  
  // Initialize ECG timing variables
  timer = millis();
  instance1 = micros();
}

void loop() {
  // read the value from the sensor:
  sensorValue = analogRead(sensorPin);
  
  // Check for abnormal sensor values
  if (sensorValue < MIN_SENSOR_VALUE || sensorValue > MAX_SENSOR_VALUE) {
    debugSerial.print("ABNORMAL SENSOR VALUE: ");
    debugSerial.println(sensorValue);
  }
  
  // ECG R-peak detection logic (using raw sensor values)
  if((sensorValue > threshold) && (!flag)) {
    count++;  
    debugSerial.println("R peak detected");
    flag = 1;
    interval = micros() - instance1; // RR interval
    instance1 = micros(); 
  }
  else if((sensorValue < threshold)) {
    flag = 0;
  }
  
  // Calculate heart rate every 10 seconds
  if ((millis() - timer) > 10000) {
    hr = count * 6; // Convert to BPM (beats per minute)
    timer = millis();
    count = 0; 
    
    // Check for abnormal heart rate
    if (hr < MIN_NORMAL_HR && hr > 0) {
      debugSerial.print("ABNORMAL HR - BRADYCARDIA: ");
      debugSerial.println(hr);
    }
    else if (hr > MAX_NORMAL_HR) {
      debugSerial.print("ABNORMAL HR - TACHYCARDIA: ");
      debugSerial.println(hr);
    }
  }
  
  // Calculate HRV
  hrv = hr/60 - interval/1000000;
  
  // Check for abnormal HRV values
  if (hrv < MIN_NORMAL_HRV || hrv > MAX_NORMAL_HRV) {
    debugSerial.print("ABNORMAL HRV: ");
    debugSerial.println(hrv);
  }
  
  int x = 0;
  int y = 512;
  // debugSerial.print(x);
  // debugSerial.print(" ");
  // debugSerial.print(y);
  // debugSerial.print(" ");
  debugSerial.print(sensorValue);
  debugSerial.print(" HR: ");
  debugSerial.print(hr);
  debugSerial.print(" HRV: ");
  debugSerial.println(hrv);

  // Send data via LoRaWAN every 30 seconds
  if (millis() - lastTransmission > transmissionRate) {
    byte payload[6];
    
    // Pack raw sensor value (2 bytes)
    payload[0] = highByte(sensorValue);
    payload[1] = lowByte(sensorValue);
    
    // Pack heart rate (2 bytes)
    int hrInt = (int)(hr * 10);
    payload[2] = highByte(hrInt);
    payload[3] = lowByte(hrInt);
    
    // Pack HRV (2 bytes)
    int hrvInt = (int)(hrv * 1000);
    payload[4] = highByte(hrvInt);
    payload[5] = lowByte(hrvInt);

    ttn.sendBytes(payload, sizeof(payload));
    lastTransmission = millis();
  }

  delay(dataRate);
}