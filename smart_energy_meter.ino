#include <Arduino.h>
#include <vector>

// ==============================================================================
// 1. CONFIGURATION PARAMETERS 
// ==============================================================================

int day_unit_seconds = 10;   // Length of one "day" cycle in SECONDS
int training_days_total = 1; // Total days to train

const int SENSOR_PIN = 34;   // ESP32 ADC Pin 
const int RELAY_PIN = 26;    // ESP32 Relay Pin (Main Light)
const int RELAY2_PIN = 14;   // ESP32 Relay Pin (Second Channel / Signal Device)

// Relay Settings
const int RELAY_TRIGGER_OFF = HIGH; 
const int RELAY_NORMAL = LOW;       

float OFF_VTG_VALUE = 3.3;
float ON_VTG_VALUE = 2.15;
float VTG_TOLERANCE = 0.40;  // +/- 0.40V tolerance to handle electrical noise

// ==============================================================================
// 2. INTERNAL VARIABLES 
// ==============================================================================

unsigned long startTime = 0;
bool isTrainingComplete = false;
int lastProcessedSecond = -1; 

std::vector<std::vector<bool>> training_data; 
std::vector<bool> processed_off_pattern;      

int secondsPerDay;

// --- OVERRIDE VARIABLES ---
bool lastSwitchState = false;
unsigned long lastOffTime = 0;
bool overrideActive = false;
const unsigned long OVERRIDE_WINDOW_MS = 6000; 

void setup() {
  Serial.begin(115200);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  
  digitalWrite(RELAY_PIN, RELAY_NORMAL); 
  digitalWrite(RELAY2_PIN, RELAY_NORMAL); 
  
  analogReadResolution(12);

  secondsPerDay = day_unit_seconds;
  
  training_data.resize(training_days_total, std::vector<bool>(secondsPerDay, false));
  processed_off_pattern.resize(secondsPerDay, false);

  Serial.println("=========================================");
  Serial.println("SMART ENERGY METER INITIALIZED (FAST TEST MODE)");
  Serial.print("Day Unit Length: "); Serial.print(day_unit_seconds); Serial.println(" seconds");
  Serial.println("Starting Training Phase...");
  Serial.println("=========================================");

  startTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long elapsedMillis = currentMillis - startTime;
  unsigned long dayDurationMillis = (unsigned long)day_unit_seconds * 1000UL;

  int currentDay = elapsedMillis / dayDurationMillis;
  int currentSecondInDay = (elapsedMillis % dayDurationMillis) / 1000;

  if (currentSecondInDay != lastProcessedSecond) {
    lastProcessedSecond = currentSecondInDay; 

    // 1. Read Sensor
    int adcValue = analogRead(SENSOR_PIN);
    float voltage = adcValue * (3.3 / 4095.0); 
    
    // Default to the previous state to prevent flickering in case of weird sensor readings
    bool isSwitchOn = lastSwitchState; 
    
    // Check if voltage is within the ON tolerance band (e.g., 1.75V to 2.55V)
    if (voltage >= (ON_VTG_VALUE - VTG_TOLERANCE) && voltage <= (ON_VTG_VALUE + VTG_TOLERANCE)) {
      isSwitchOn = true;
    } 
    // Check if voltage is within the OFF tolerance band (e.g., 2.90V to 3.70V)
    else if (voltage >= (OFF_VTG_VALUE - VTG_TOLERANCE) && voltage <= (OFF_VTG_VALUE + VTG_TOLERANCE)) {
      isSwitchOn = false;
    }

    // 2. OVERRIDE DETECTION
    if (lastSwitchState == true && isSwitchOn == false) {
      lastOffTime = millis();
      overrideActive = false; 
    } 
    else if (lastSwitchState == false && isSwitchOn == true) {
      if (millis() - lastOffTime <= OVERRIDE_WINDOW_MS) {
        overrideActive = true; 
      } else {
        overrideActive = false; 
      }
    }
    lastSwitchState = isSwitchOn;

    // 3. TRAINING PHASE
    if (currentDay < training_days_total) {
      training_data[currentDay][currentSecondInDay] = isSwitchOn;

      Serial.print("[TRAINING] Day: "); Serial.print(currentDay + 1);
      Serial.print(" | Sec: "); Serial.print(currentSecondInDay); Serial.print(" / "); Serial.print(secondsPerDay - 1);
      Serial.print(" | Volts: "); Serial.print(voltage); 
      Serial.print("V -> SAVED AS: "); Serial.println(isSwitchOn ? "ON" : "OFF");
    }

    // 4. PROCESS DATA 
    else if (currentDay == training_days_total && !isTrainingComplete) {
      Serial.println("\n=========================================");
      Serial.println("TRAINING COMPLETE! Processing Data...");
      
      for (int sec = 0; sec < secondsPerDay; sec++) {
        int offCount = 0;
        for (int d = 0; d < training_days_total; d++) {
          if (training_data[d][sec] == false) { offCount++; }
        }
        
        if (offCount >= (training_days_total / 2 + 1)) {
          processed_off_pattern[sec] = true; 
        } else {
          processed_off_pattern[sec] = false;
        }
      }
      
      Serial.print("FINAL LEARNED PATTERN: [");
      for(int i = 0; i < secondsPerDay; i++) {
        Serial.print(processed_off_pattern[i] ? "OFF" : "ON_or_IGNORE");
        if(i < secondsPerDay - 1) Serial.print(", ");
      }
      Serial.println("]");
      
      isTrainingComplete = true;
      Serial.println("Starting Active Operation...");
      Serial.println("=========================================\n");
    }

    // 5. ACTIVE OPERATION PHASE 
    if (isTrainingComplete) {
      bool shouldBeOff = processed_off_pattern[currentSecondInDay];

      if (shouldBeOff == false && overrideActive == true) {
        overrideActive = false;
      }

      Serial.print("[OPERATING] Sec: "); Serial.print(currentSecondInDay);
      Serial.print(" | Switch: "); Serial.print(isSwitchOn ? "ON" : "OFF");
      Serial.print(" | Rule: "); Serial.print(shouldBeOff ? "MUST BE OFF" : "NO RULE");
      Serial.print(" | Override: "); Serial.println(overrideActive ? "ACTIVE" : "NONE");

      // ==============================================================================
      // RELAY CONTROL LOGIC (Upgraded for clarity and crash-prevention)
      // ==============================================================================
      if (shouldBeOff == true) {
        if (isSwitchOn == true) {
          if (overrideActive == false) {
            // SCENARIO A: Human left it ON during an OFF period. Trigger Relays!
            digitalWrite(RELAY_PIN, RELAY_TRIGGER_OFF);  
            delay(100); // 100ms delay to prevent the power-spike crash from dual coils
            digitalWrite(RELAY2_PIN, RELAY_TRIGGER_OFF); 
            
            Serial.println("   >>> ACTION: Pattern says OFF, but switch is ON! Cutting Power!");
          } else {
            // SCENARIO B: Human did the manual override toggle.
            digitalWrite(RELAY_PIN, RELAY_NORMAL);  
            digitalWrite(RELAY2_PIN, RELAY_NORMAL);
            Serial.println("   >>> NO ACTION: Manual Override is ACTIVE. Letting light stay ON.");
          }
        } else {
          // SCENARIO C: Switch is physically OFF.
          digitalWrite(RELAY_PIN, RELAY_NORMAL);  
          digitalWrite(RELAY2_PIN, RELAY_NORMAL);
          Serial.println("   >>> NO ACTION: Switch is ALREADY physically OFF. All good!");
        }
      } else {
        // SCENARIO D: Time period has no OFF rules.
        digitalWrite(RELAY_PIN, RELAY_NORMAL);  
        digitalWrite(RELAY2_PIN, RELAY_NORMAL);
      }
    }
  }
}