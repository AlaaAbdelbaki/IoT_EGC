#include <TheThingsNetwork.h>

int sensorPin = A0;   // select the input pin for the ECG sensor
int sensorValue = 0;  // variable to store the value coming from the sensor

// TODO: Fill these with values from your The Things Network console
// These are strings, usually found in your TTN application or device overview.
const char *appEui = ""; // Your Application EUI
const char *appKey = ""; // Your Application Key

// Replace REPLACE_ME with TTN_FP_EU868 (Europe) or TTN_FP_US915 (North America)
// or the appropriate frequency plan for your region.
#define freqPlan TTN_FP_EU868 

#define loraSerial Serial1 // Typically Serial1 for LoRaWAN modules on Leonardo
#define debugSerial Serial  // Standard debug serial port

// ECG monitoring variables and thresholds
long instance1=0, timer;
double hrv =0, hr = 72; // Initial heart rate value, will be updated
int count = 0;  
bool flag = 0;
#define threshold 300 // R-peak detection threshold (adjusted for 0-1023 range)
#define timer_value 10000 // 10 seconds timer to calculate hr (in milliseconds)
double interval = 0; // RR interval in microseconds

// Abnormal value detection thresholds (from IoT.ino)
#define MIN_NORMAL_HR 40    // Minimum normal heart rate (bradycardia below this)
#define MAX_NORMAL_HR 180   // Maximum normal heart rate (tachycardia above this)
#define MAX_NORMAL_HRV 100  // Maximum reasonable HRV value
#define MIN_NORMAL_HRV -100 // Minimum reasonable HRV value
#define MIN_SENSOR_VALUE 0  // Minimum sensor reading
#define MAX_SENSOR_VALUE 1023 // Maximum sensor reading

// The time to wait after each loop
#define dataRate 10 // Delay in milliseconds after each loop iteration
#define transmissionRate 30000 // Send data every 30 seconds (in milliseconds)

unsigned long lastTransmission = 0;

TheThingsNetwork ttn(loraSerial, debugSerial, freqPlan);

void setup() {
  loraSerial.begin(57600); // Baud rate for your LoRaWAN module
  debugSerial.begin(9600); // Baud rate for Serial Monitor

  // Wait a maximum of 10s for Serial Monitor to open
  while (!debugSerial && millis() < 10000);

  debugSerial.println("-- STATUS");
  ttn.showStatus(); // Show current status of the LoRaWAN module

  debugSerial.println("-- JOIN");
  // Attempt to join the LoRaWAN network
  ttn.join(appEui, appKey); 
  
  // Initialize ECG timing variables
  timer = millis();     // Start timer for HR calculation
  instance1 = micros(); // Start microsecond timer for RR interval
}

void loop() {
  // Read the value from the ECG sensor:
  sensorValue = analogRead(sensorPin);
  
  // Check for abnormal sensor values (0-1023 range)
  if (sensorValue < MIN_SENSOR_VALUE || sensorValue > MAX_SENSOR_VALUE) {
    debugSerial.print("ABNORMAL SENSOR VALUE: ");
    debugSerial.println(sensorValue);
  }
  
  // ECG R-peak detection logic (based on sensor value exceeding a threshold)
  if((sensorValue > threshold) && (!flag)) {
    count++;  
    debugSerial.println("R peak detected");
    flag = 1;
    // Calculate RR interval in microseconds
    interval = micros() - instance1; 
    instance1 = micros(); // Reset R-peak timer
  }
  else if((sensorValue < threshold)) {
    flag = 0; // Reset flag when sensor value drops below threshold
  }
  
  // Calculate heart rate (HR) every timer_value (10 seconds)
  if ((millis() - timer) > timer_value) {
    hr = (double)count * (60000.0 / timer_value); // Convert count over time_value to BPM
    timer = millis(); // Reset HR calculation timer
    count = 0;        // Reset R-peak count
    
    // Check for abnormal heart rate (bradycardia or tachycardia)
    if (hr < MIN_NORMAL_HR && hr > 0) {
      debugSerial.print("ABNORMAL HR - BRADYCARDIA: ");
      debugSerial.println(hr);
    }
    else if (hr > MAX_NORMAL_HR) {
      debugSerial.print("ABNORMAL HR - TACHYCARDIA: ");
      debugSerial.println(hr);
    }
  }
  
  // Calculate Heart Rate Variability (HRV) - simplified from IoT.ino
  // Note: A more robust HRV calculation typically involves a series of R-R intervals.
  // This is a basic approach.
  if (hr > 0 && interval > 0) { // Avoid division by zero
    hrv = (hr/60.0) - (interval/1000000.0); // Difference between avg HR interval and current RR interval
  } else {
    hrv = 0;
  }
  
  // Check for abnormal HRV values
  if (hrv < MIN_NORMAL_HRV || hrv > MAX_NORMAL_HRV) {
    debugSerial.print("ABNORMAL HRV: ");
    debugSerial.println(hrv);
  }
  
  // Debug output to Serial Monitor
  debugSerial.print("ECG: ");
  debugSerial.print(sensorValue);
  debugSerial.print(" HR: ");
  debugSerial.print(hr);
  debugSerial.print(" BPM, HRV: ");
  debugSerial.print(hrv);
  debugSerial.println(" s");

  // Send data via LoRaWAN every transmissionRate (30 seconds)
  if (millis() - lastTransmission > transmissionRate) {
    byte payload[6];
    
    // Pack raw sensor value (2 bytes)
    payload[0] = highByte(sensorValue);
    payload[1] = lowByte(sensorValue);
    
    // Pack heart rate (2 bytes) - scaled by 10 to preserve one decimal place
    int hrInt = (int)(hr * 10);
    payload[2] = highByte(hrInt);
    payload[3] = lowByte(hrInt);
    
    // Pack HRV (2 bytes) - scaled by 1000 to preserve three decimal places
    int hrvInt = (int)(hrv * 1000);
    payload[4] = highByte(hrvInt);
    payload[5] = lowByte(hrvInt);

    ttn.sendBytes(payload, sizeof(payload));
    debugSerial.println("LoRaWAN packet sent.");
    lastTransmission = millis();
  }

  delay(dataRate); // Small delay to stabilize readings and prevent busy-waiting
}