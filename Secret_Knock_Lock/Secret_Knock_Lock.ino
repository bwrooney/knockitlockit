#include <AM2320.h>
#include <MakerBoard.h>
#include <PCA9555.h>
#include <Adafruit_NeoPixel.h>
#include <Servo.h>

/* Secret Knock Lock Box
   Piezo element detects knock and opens if the lock if the sequence is correct
*/

// Pins
const int piezo = 2;
Adafruit_NeoPixel ring = Adafruit_NeoPixel(16, 6, NEO_GRB + NEO_KHZ800); // Pin 6
#define BUZZER 10

// Constants
const int threshold = 25; // Detects knock if piezo returns value higher than this
const int debounce = 100; //Knock debounce time to avoid recording the same knock twice
const int maxKnocks = 25; // Max knocks for a password
const int waitTime = 3000; // Max wait time before evaluating input
const int pcntWrong = 50; // Incorrectness of indivual lock
const int avgPcntWrong = 25;  // Incorrectness of entire sequence

// Password Stuff
int secretKnock[maxKnocks] = {50, 25, 25, 50, 100, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Holds the sequence of knocks for passowrd. The numbers represent the time that has passed since the last knock
int secretKnockLen = 6; // Number of knocks in secret knock
int inputKnock[maxKnocks]; // Array of knocks to compare to secretKnock
int knock = 0; // Current piezo reading
boolean correct = true; // If the password entered was correct, used for setting new password
boolean edit = false; // used for editing
byte state[2] = {1,1};
byte prev = 0;

Servo myservo;  // create servo object to control a servo
int pos = 0;    // variable to store the servo position

void setup() {
  Serial.begin(9600);
  ring.begin();
  ring.setBrightness(16);
  ring.clear();
  ring.show();
  pinMode(BUZZER, OUTPUT);
  mb.begin();
  myservo.attach(11);  // attaches the servo on pin 11
  myservo.write(1);
}

void loop() {
  knock = analogRead(piezo);  //Listen for knock
  byte s = mb.getButton(0);  // Button for editing knock
  byte t = mb.getButton(2);  // Button for reseting Servo
  if(mb.getButton(2) == 0){
     if (millis() > prev + 1000) {
        myservo.write(90);
        delay(100);
        prev = millis();
     }
  }
  if (correct && mb.getButton(0) == 0) {
    if (millis() > prev + 1000) {
      Serial.println("Input New Knock");
      editLight();
      delay(500);
      prev = millis();
      edit = true;
      recordKnock();
    }
  }
  if (knock >= threshold) {
    listen4Knock();
  }
}

void clearInput() {
  for (int i = 0; i < maxKnocks; i++) {  //clear knocks from last input
    inputKnock[i] = 0;
  }
}

void recordKnock() {
  int numKnocks = 0;
  int start = millis();
  int currKnock = millis();
  Serial.println("Begin Knock");
  clearInput();
  delay(debounce);
  while ((numKnocks < maxKnocks) && (millis() - start < waitTime)) { // Record sequence of knocks while the number of knocks is less than max and waitTime hasn't expired
    knock = analogRead(piezo);
    if (knock >= threshold) {
      currKnock = millis();
      inputKnock[numKnocks] = currKnock - start;
      Serial.print("Knock #");
      Serial.println(numKnocks + 1);
      Serial.print("Reading: ");
      Serial.println(inputKnock[numKnocks]);
      numKnocks++;
      start = currKnock;
      delay(debounce);
    }
    currKnock = millis();
  }
  if (edit) {
    setLock();
  }
}

void setLock() {
  int size = 0;
  normalize();
  for (int i = 1; i < maxKnocks; i ++) {
    secretKnock[i - 1] = inputKnock[i];
    if (inputKnock[i] > 0) {
      size ++;
    }
  }
  secretKnockLen = size;
  editLight();
  Serial.println("New Lock Set");
  clearAll();
}

void clearAll() {
  clearInput();
  correct = false;
  edit = false;
  state[0] = 1;
  state[2] = 1;
  prev = 0;
}

void listen4Knock() {
  recordKnock();
  Serial.println("Knock Recorded");
  if (checkInput()) {
    correct = true;
    lockOpen();
  }
  else {
    correct = false;
    lockWrong();
  }
}

void normalize() {
  int maxInterval = 0;
  for (int i = 0; i < maxKnocks; i ++) {
    if (inputKnock[i] > maxInterval) { //record highest time between knocks
      maxInterval = inputKnock[i];
    }
  }
  for (int i = 0; i < maxKnocks; i ++) {
    inputKnock[i] = map(inputKnock[i], 0, maxInterval, 0, 100);
  }
}

boolean checkInput() {
  int inputTotalKnocks = 0;
  int totalDiff = 0;
  int maxInterval = 0;
  for (int i = 0; i < maxKnocks; i ++) { //get total number of knocks from input
    if (inputKnock[i] > 0) {
      inputTotalKnocks++;
    }
    if (inputKnock[i] > maxInterval) { //record highest time between knocks
      maxInterval = inputKnock[i];
    }
  }
  if (inputTotalKnocks != secretKnockLen) { //make sure they're the same number of knocks
    Serial.println("WRONG - not right amount of knocks");
    Serial.print("Correct Amount = ");
    Serial.println(secretKnockLen);
    Serial.print("Input Amount = ");
    Serial.println(inputTotalKnocks);
    clearAll();
    return false;
  }
  normalize();
  for (int i = 0; i < maxKnocks; i ++) {
    if (abs(inputKnock[i] - secretKnock[i]) > pcntWrong) { //check that each knock is within correct range
      Serial.println("WRONG - at least one knock does not match");
      Serial.print("Knock # ");
      Serial.println(i);
      Serial.println(inputKnock[i]);
      clearAll();
      return false;
    }
    totalDiff += abs(inputKnock[i] - secretKnock[i]);
  }
  if ((totalDiff / secretKnockLen) > avgPcntWrong) {
    Serial.println("WRONG - overall tempo does not match");
    clearAll();
    return false;
  }
  Serial.println("CORRECT");
//  clearAll();
  return true;
}

void lockOpen() {
  // make sure to wait for double knock to close it
  tone(BUZZER, 500, 1000);
  myservo.write(90);
  for (int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(0, 255, 0));
  }
  ring.show();
  delay(1000);
  for (int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(0, 0, 0));
  }
  ring.show();
}

void lockWrong() {
  // flash some lights and lock the lock
  tone(BUZZER, 250, 200);
  myservo.write(0);
  delay(10);
  tone(BUZZER, 250, 200);
  for (int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(255, 0, 0));
  }
  ring.show();
  delay(1000);
  for (int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(0, 0, 0));
  }
  ring.show();
}

void editLight() {
  tone(BUZZER, 150, 200);
  delay(10);
  tone(BUZZER, 350, 200);
  for (int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(0, 0, 255));
  }
  ring.show();
  delay(1000);
  for (int i = 0; i < ring.numPixels(); i++) {
    ring.setPixelColor(i, ring.Color(0, 0, 0));
  }
  ring.show();
}

