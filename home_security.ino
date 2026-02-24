#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

#define TRIG1 D5
#define ECHO1 D6
#define TRIG2 D7
#define ECHO2 D8

#define SERVO_PIN D4
#define LED_PIN D0
#define BUZZER_PIN D1
#define BUTTON_PIN D2

Servo doorServo;
ESP8266WebServer server(80);

bool homeLocked = true;
bool panicMode = false;

unsigned long alertStartTime = 0;
unsigned long panicStartTime = 0;
unsigned long windowAlertStart = 0;

bool doorAlertActive = false;
bool windowAlertActive = false;

const long doorThreshold = 20;     // cm
const long windowThreshold = 5;    // cm

// -------- Distance Function ----------
long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

// -------- Door Open ----------
void openDoor() {
  doorServo.write(90);
  homeLocked = false;
  server.send(200, "text/plain", "Door Opened");
}

// -------- Door Close ----------
void closeDoor() {
  doorServo.write(0);
  homeLocked = true;
  server.send(200, "text/plain", "Door Closed");
}

// -------- Panic ----------
void triggerPanic() {
  panicMode = true;
  panicStartTime = millis();
  server.send(200, "text/plain", "Panic Activated");
}

// -------- Lock / Unlock ----------
void lockHome() {
  homeLocked = true;
  server.send(200, "text/plain", "Home Locked");
}

void unlockHome() {
  homeLocked = false;
  server.send(200, "text/plain", "Home Unlocked");
}

void setup() {

  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  Serial.begin(115200);

  // -------- WiFi AP Mode ----------
  WiFi.mode(WIFI_AP);
  WiFi.softAP("HomeSecurity", "12345678");

  IPAddress local_IP(192,168,4,1);
  IPAddress gateway(192,168,4,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_IP, gateway, subnet);

  Serial.println("Access Point Started");
  Serial.println(WiFi.softAPIP());

  // -------- Server Routes ----------
  server.on("/open", openDoor);
  server.on("/close", closeDoor);
  server.on("/panic", triggerPanic);
  server.on("/lock", lockHome);
  server.on("/unlock", unlockHome);

  server.begin();
}

void loop() {

  server.handleClient();

  long doorDistance = getDistance(TRIG1, ECHO1);
  long windowDistance = getDistance(TRIG2, ECHO2);
// -------- Door Detection ----------
static bool personDetected = false;

if (doorDistance < doorThreshold) {

  if (!personDetected) {
    personDetected = true;

    if (homeLocked) {
      // Locked → 7 second alarm
      doorAlertActive = true;
      alertStartTime = millis();
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
    } 
    else {
      // Door open → single short beep
      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

} else {
  personDetected = false;
}

// Stop 7 sec locked alarm
if (doorAlertActive && millis() - alertStartTime > 7000) {
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  doorAlertActive = false;
}

  // -------- Window Detection ----------
  if (windowDistance < windowThreshold && !windowAlertActive) {
    windowAlertActive = true;
    windowAlertStart = millis();
  }

  if (windowAlertActive) {
    if ((millis() - windowAlertStart) % 1000 < 200) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }

    if (millis() - windowAlertStart > 7000) {
      digitalWrite(BUZZER_PIN, LOW);
      windowAlertActive = false;
    }
  }

  // -------- Panic Button ----------
  if (digitalRead(BUTTON_PIN) == LOW && !panicMode) {
    panicMode = true;
    panicStartTime = millis();
  }

  if (panicMode) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);

    if (millis() - panicStartTime > 10000) {
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      panicMode = false;
    }
  }
}
