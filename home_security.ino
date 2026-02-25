#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266Servo.h>  

// ---------------- PIN DEFINITIONS ----------------
#define TRIG1 D5
#define ECHO1 D6
#define TRIG2 D7
#define ECHO2 D8

#define SERVO_PIN D4
#define LED_PIN D0
#define BUZZER_PIN D1
#define BUTTON_PIN D2

// ---------------- OBJECTS ----------------
Servo doorServo;
ESP8266WebServer server(80);

// ---------------- VARIABLES ----------------
bool homeLocked = true;
bool panicMode = false;

bool doorAlertActive = false;
bool windowAlertActive = false;

unsigned long alertStartTime = 0;
unsigned long panicStartTime = 0;
unsigned long windowAlertStart = 0;

const long doorThreshold = 20;
const long windowThreshold = 5;

// ---------------- DISTANCE FUNCTION ----------------
long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  long distance = duration * 0.034 / 2;

  return distance;
}

// ---------------- DOOR CONTROL ----------------
void openDoor() {
  doorServo.write(90);
  homeLocked = false;
  server.send(200, "text/plain", "Door Opened");
}

void closeDoor() {
  doorServo.write(0);
  homeLocked = true;
  server.send(200, "text/plain", "Door Closed");
}

void lockHome() {
  homeLocked = true;
  server.send(200, "text/plain", "Home Locked");
}

void unlockHome() {
  homeLocked = false;
  server.send(200, "text/plain", "Home Unlocked");
}

void triggerPanic() {
  panicMode = true;
  panicStartTime = millis();
  server.send(200, "text/plain", "Panic Activated");
}

// ---------------- SETUP ----------------
void setup() {

  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);  // OFF (Active LOW buzzer)

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  Serial.begin(115200);

  // -------- WiFi AP Mode --------
  WiFi.mode(WIFI_AP);
  WiFi.softAP("HomeSecurity", "12345678");

  IPAddress local_IP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_IP, gateway, subnet);

  Serial.println("Access Point Started");
  Serial.println(WiFi.softAPIP());

  // -------- Server Routes --------
  server.on("/open", openDoor);
  server.on("/close", closeDoor);
  server.on("/panic", triggerPanic);
  server.on("/lock", lockHome);
  server.on("/unlock", unlockHome);

  server.begin();
}

// ---------------- LOOP ----------------
void loop() {

  server.handleClient();

  long doorDistance = getDistance(TRIG1, ECHO1);
  long windowDistance = getDistance(TRIG2, ECHO2);

  static bool personDetected = false;

  // -------- DOOR SENSOR --------
  if (doorDistance > 0 && doorDistance < doorThreshold) {

    if (!personDetected) {
      personDetected = true;

      if (homeLocked) {
        doorAlertActive = true;
        alertStartTime = millis();
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(BUZZER_PIN, LOW);   // ON
      } 
      else {
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
        digitalWrite(BUZZER_PIN, HIGH);
      }
    }

  } else {
    personDetected = false;
  }

  if (doorAlertActive && millis() - alertStartTime > 7000) {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);
    doorAlertActive = false;
  }

  // -------- WINDOW SENSOR --------
  if (windowDistance > 0 && windowDistance < windowThreshold && !windowAlertActive) {
    windowAlertActive = true;
    windowAlertStart = millis();
  }

  if (windowAlertActive) {

    if ((millis() - windowAlertStart) % 1000 < 200) {
      digitalWrite(BUZZER_PIN, LOW);
    } else {
      digitalWrite(BUZZER_PIN, HIGH);
    }

    if (millis() - windowAlertStart > 7000) {
      digitalWrite(BUZZER_PIN, HIGH);
      windowAlertActive = false;
    }
  }

  // -------- PANIC BUTTON --------
  if (digitalRead(BUTTON_PIN) == LOW && !panicMode) {
    panicMode = true;
    panicStartTime = millis();
  }

  if (panicMode) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, LOW);

    if (millis() - panicStartTime > 10000) {
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, HIGH);
      panicMode = false;
    }
  }
}
