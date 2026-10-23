#include <SPI.h> 
#include <nRF24L01.h> 
#include <RF24.h> 
 
#define CE_PIN  7 
#define CSN_PIN 8 
#define ENA     5 
#define ENB     6 
#define IN1     9 
#define IN2     10 
#define IN3     A4 
#define IN4     A5 
#define FRONT_LIGHT     A0 
#define BACK_LIGHT      A1 
#define RIGHT_INDICATOR A2 
#define LEFT_INDICATOR  A3 
#define BUZZER          4 
#define LED_LINK        3 
 
RF24 radio(CE_PIN, CSN_PIN); 
const byte address[6] = "00001"; 
 
struct Payload { 
  uint16_t throttle; 
  uint16_t steering; 
  uint16_t speedPot; 
  uint8_t  ch3, ch4, ch5; 
  uint8_t  toggleBuzzer; 
} data; 
 
bool frontLightOn = false, backLightManual = false, hazardOn = false; 
bool leftIndOn = false, rightIndOn = false, reversing = false; 
bool buzzerOn = false; 
int currentSpeed = 0; 
unsigned long lastSignal = 0, lastBlink = 0; 
bool blinkState = false; 
bool everConnected = false; 
 
const int SPEED_TABLE[10] = {0, 28, 56, 85, 113, 142, 170, 198, 227, 255}; 
 
void setup() { 

  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT); 
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); 
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT); 
  pinMode(FRONT_LIGHT, OUTPUT); pinMode(BACK_LIGHT, OUTPUT); 
  pinMode(RIGHT_INDICATOR, OUTPUT); pinMode(LEFT_INDICATOR, OUTPUT); 
  pinMode(BUZZER, OUTPUT); 
  pinMode(LED_LINK, OUTPUT); 
 
  radio.begin(); 
  radio.openReadingPipe(1, address); 
  radio.setPALevel(RF24_PA_MAX); 
  radio.setDataRate(RF24_250KBPS); 
  radio.startListening(); 
 
  stopCar(); 
  allIndicatorsOff(); 
  digitalWrite(LED_LINK, LOW); 
  digitalWrite(BUZZER, LOW); 
 
  Serial.begin(9600); 
  Serial.println("CAR: ALL 1-PRESS TOGGLE FIXED"); 
} 
 
void loop() { 
  if (radio.available()) { 
    radio.read(&data, sizeof(data)); 
    lastSignal = millis(); 
    everConnected = true; 
    digitalWrite(LED_LINK, HIGH); 
 
    // === SPEED === 
    int level = map(data.speedPot, 0, 865, 0, 9); 
    level = constrain(level, 0, 9); 
    currentSpeed = SPEED_TABLE[level]; 
 
    // === THROTTLE & STEERING === 
    int throttle = map(data.throttle, 0, 1023, 255, -255); 
    if (abs(throttle) < 80) throttle = 0; 
    int steer = map(data.steering, 0, 1023, -255, 255); 
    if (abs(steer) < 80) steer = 0; 
 
    int leftMotor  = throttle + steer; 
    int rightMotor = throttle - steer; 
    leftMotor  = constrain(leftMotor,  -255, 255); 
    rightMotor = constrain(rightMotor, -255, 255); 
 
    int leftSpeed  = map(abs(leftMotor),  0, 255, 0, currentSpeed); 
    int rightSpeed = map(abs(rightMotor), 0, 255, 0, currentSpeed); 

 
    // === MOTORS (unchanged) === 
    if (leftMotor > 0) { 
      analogWrite(ENA, leftSpeed); digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); 
    } else if (leftMotor < 0) { 
      analogWrite(ENA, leftSpeed); digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); 
    } else { 
      analogWrite(ENA, 0); digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); 
    } 
 
    if (rightMotor > 0) { 
      analogWrite(ENB, rightSpeed); digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); 
    } else if (rightMotor < 0) { 
      analogWrite(ENB, rightSpeed); digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); 
    } else { 
      analogWrite(ENB, 0); digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); 
    } 
 
    // === TOGGLE BUZZER === 
    static bool lastToggle = 0; 
    if (data.toggleBuzzer && !lastToggle) { 
      buzzerOn = !buzzerOn; 
      tone(BUZZER, 2000, 100); 
    } 
    lastToggle = data.toggleBuzzer; 
 
    // === REVERSE === 
    reversing = (throttle < -50); 
 
    // === BUZZER: REVERSE OR TOGGLE ON === 
    digitalWrite(BUZZER, (reversing || buzzerOn) ? HIGH : LOW); 
 
    // === LIGHTS: CLEAN TOGGLE === 
    static uint8_t prevCh3 = 0, prevCh4 = 0, prevCh5 = 0; 
    if (data.ch3 != prevCh3) { frontLightOn = data.ch3; digitalWrite(FRONT_LIGHT, 
frontLightOn); } 
    if (data.ch4 != prevCh4) { hazardOn = data.ch4; if (!hazardOn) allIndicatorsOff(); } 
    if (data.ch5 != prevCh5) { backLightManual = data.ch5; } 
    prevCh3 = data.ch3; prevCh4 = data.ch4; prevCh5 = data.ch5; 
 
    digitalWrite(BACK_LIGHT, (reversing || backLightManual) ? HIGH : LOW); 
 
    // === INDICATORS === 
    leftIndOn  = (steer < -100); 
    rightIndOn = (steer > 100); 
  } 
  else if (millis() - lastSignal > 1000) { 
    stopCar(); 

    digitalWrite(BUZZER, LOW); buzzerOn = false; 
    allIndicatorsOff(); 
    digitalWrite(FRONT_LIGHT, LOW); digitalWrite(BACK_LIGHT, LOW); 
    digitalWrite(LED_LINK, (millis() % 200 < 100) ? HIGH : LOW); 
  } 
 
  // === INDICATOR BLINK === 
  if (millis() - lastBlink >= 400) { 
    lastBlink = millis(); 
    blinkState = !blinkState; 
    if (hazardOn && !carMoving()) { 
      digitalWrite(LEFT_INDICATOR, blinkState); 
      digitalWrite(RIGHT_INDICATOR, blinkState); 
    } 
    else if (leftIndOn)  digitalWrite(LEFT_INDICATOR, blinkState); 
    else if (rightIndOn) digitalWrite(RIGHT_INDICATOR, blinkState); 
    else allIndicatorsOff(); 
  } 
} 
 
void stopCar() { 
  analogWrite(ENA, 0); analogWrite(ENB, 0); 
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); 
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); 
  digitalWrite(BUZZER, LOW); 
  reversing = false; 
} 
bool carMoving() { return (digitalRead(IN1) || digitalRead(IN2) || digitalRead(IN3) || 
digitalRead(IN4)); } 
void allIndicatorsOff() { digitalWrite(LEFT_INDICATOR, LOW); 
digitalWrite(RIGHT_INDICATOR, LOW); } 
 
 
 
CODE FOR TX  TRANSMITTER REMOTE 
 
 
 
#include <SPI.h> 
#include <nRF24L01.h> 
#include <RF24.h> 
 
#define CE_PIN   7 
#define CSN_PIN  8 
 
// Physical analog pins you used previously (keep for reference) 
#define JOY1_Y   A0   // was used for throttle previously 
#define JOY2_X   A3   // was used for steering previously 

#define POT      A2 
 
// Logical mapping — set these to the analog pins that match how you wired VRx / VRy 
// Example: 
//   If Joystick1 VRx is on A0, set THROTTLE_PIN to A0. 
//   If Joystick2 VRY is on A3, set STEERING_PIN to A3. 
#define THROTTLE_PIN JOY1_Y 
#define STEERING_PIN JOY2_X 
 
// If directions are opposite, flip these to true 
const bool INVERT_THROTTLE = false; 
const bool INVERT_STEERING = false; 
 
#define JOY1_SW  2   // Front Light 
#define JOY2_SW  9   // Toggle Buzzer 
#define TOUCH1   3   // Hazard 
#define TOUCH2   4   // Back Light 
#define BUZZER   5 
#define LED_LINK 6 
 
RF24 radio(CE_PIN, CSN_PIN); 
const byte address[6] = "00001"; 
 
struct Payload { 
  uint16_t throttle; 
  uint16_t steering; 
  uint16_t speedPot; 
  uint8_t  ch3, ch4, ch5;     // Front, Hazard, Back 
  uint8_t  toggleBuzzer;      // JOY2_SW 
} data; 
 
// Button states 
bool prevJoy1Sw = HIGH, prevJoy2Sw = HIGH; 
bool prevT1 = LOW, prevT2 = LOW; 
int prevPot = -1; 
unsigned long lastDebounce = 0; 
const unsigned long DEBOUNCE_DELAY = 50;  // ← REDUCED TO 50ms 
 
void setup() { 
  Serial.begin(9600); 
  pinMode(JOY1_SW, INPUT_PULLUP); 
  pinMode(JOY2_SW, INPUT_PULLUP); 
  pinMode(TOUCH1, INPUT); 
  pinMode(TOUCH2, INPUT); 
  pinMode(BUZZER, OUTPUT); 
  pinMode(LED_LINK, OUTPUT); 
 
  radio.begin(); 

  radio.openWritingPipe(address); 
  radio.setPALevel(RF24_PA_MAX); 
  radio.setDataRate(RF24_250KBPS); 
  radio.stopListening(); 
 
  digitalWrite(LED_LINK, HIGH); delay(300); digitalWrite(LED_LINK, LOW); 
  Serial.println("REMOTE: ALL BUTTONS 1-PRESS TOGGLE"); 
} 
 
void loop() { 
  unsigned long now = millis(); 
 
  // Read logical throttle & steering pins (so you can re-map in one place) 
  int rawThrottle = analogRead(THROTTLE_PIN); 
  int rawSteering = analogRead(STEERING_PIN); 
 
  // Optionally invert if joystick direction is opposite 
  if (INVERT_THROTTLE) rawThrottle = 1023 - rawThrottle; 
  if (INVERT_STEERING) rawSteering = 1023 - rawSteering; 
 
  data.throttle = rawThrottle; 
  data.steering = rawSteering; 
 
  int potVal = constrain(analogRead(POT), 0, 865); 
  data.speedPot = potVal; 
 
  bool joy1Sw = digitalRead(JOY1_SW) == LOW; 
  bool joy2Sw = digitalRead(JOY2_SW) == LOW; 
  bool t1 = digitalRead(TOUCH1); 
  bool t2 = digitalRead(TOUCH2); 
 
  // === CLEAN EDGE DETECTION (ALL BUTTONS) === 
  if (now - lastDebounce > DEBOUNCE_DELAY) { 
    // JOY1_SW → Front Light 
    if (joy1Sw && !prevJoy1Sw) { 
      data.ch3 = !data.ch3; 
      tone(BUZZER, 1600, 80); 
      lastDebounce = now; 
    } 
    // JOY2_SW → Toggle Buzzer 
    if (joy2Sw && !prevJoy2Sw) { 
      data.toggleBuzzer = 1; 
      tone(BUZZER, 1800, 80); 
      lastDebounce = now; 
    } else { 
      data.toggleBuzzer = 0; 
    } 
    // TOUCH1 → Hazard 

    if (t1 && !prevT1) { 
      data.ch4 = !data.ch4; 
      tone(BUZZER, 900, 80); 
      lastDebounce = now; 
    } 
    // TOUCH2 → Back Light 
    if (t2 && !prevT2) { 
      data.ch5 = !data.ch5; 
      tone(BUZZER, 700, 80); 
      lastDebounce = now; 
    } 
  } 
 
  prevJoy1Sw = joy1Sw; prevJoy2Sw = joy2Sw; 
  prevT1 = t1; prevT2 = t2; 
 
  if (abs(potVal - prevPot) > 10) { tone(BUZZER, 1100, 30); prevPot = potVal; } 
 
  bool sent = radio.write(&data, sizeof(data)); 
  digitalWrite(LED_LINK, sent); 
 
  delay(20);  // Faster loop 
}