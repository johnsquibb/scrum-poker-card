// Instructions for Flashing bootloader and installing to Scrum Poker Card Rev 1.2
// 1. Using Arduino Uno or similar, follow the wiring guides for connecting programmer to target ICSP header at https://www.arduino.cc/en/tutorial/arduinoISP.
// 2. In Arduino IDE, Open the "ArduinoISP" sketch, then select "Arduino as ISP" as programmer in Tools menu.
// 3. Upload the Arduino ISP sketch onto the programmer device (Uno).
// 4. While holding down the Reset button on the target device (Scrum Poker Card), select Tools->Burn Bootloader. Ensure the reset button is held until the console/avrdude
// shows writing progress (It is unncessary to hold reset one writing has started, but continuing to hold will be fine). You should see flashing lights on the programmer 
// and target as the bootloader is flashed, and avrdude should show success message in console output.
// 5. Remove the ICSP header connections from target.
// 6. Using a 5V FTDI programmer cable, connect the Scrum Poker Card to Arduino IDE. Set the Tools->Programmer to "AVRISP mkII" and Board to "Arduino Uno". 
// 7. Upload this sketch to the Scrum Poker Card over port connected to FTDI programmer cable.
// 8. Done!



// Copyright 2020 John Squibb
// Scrum poker points display.
// Various other functions fill up the available 4-bit instruction set.
// Configuration for common anode 7-segment display.
// Thanks to:
// https://www.jameco.com/Jameco/workshop/TechTip/working-with-seven-segment-displays.html
// https://gist.github.com/baojie/4522173

// Digital pins
#define LED_A 2
#define LED_B 3
#define LED_C 4
#define LED_D 5
#define LED_P 6
#define LED_MSD_ENABLE 7
#define LED_LSD_ENABLE 8 
#define BUZZER 9
#define BUTTON 10

// Analog Pins
#define DIP_1 A3
#define DIP_2 A2
#define DIP_3 A1
#define DIP_4 A0

// if analog input pin 5 is unconnected, random analog
// noise will cause the call to randomSeed() to generate
// different seed numbers each time the sketch runs.
// randomSeed() will then shuffle the random function.
#define ANALOG_PIN_FOR_RAND_SEED 5

#define POINTS_HISTORY_MAX 128

int bcdPins[] = {LED_D, LED_C, LED_B, LED_A};
int displayPins[] = {LED_LSD_ENABLE, LED_MSD_ENABLE};
int decimalPoint = LED_P;
int dipPins[] = {DIP_1, DIP_2, DIP_3, DIP_4};
int buzzerPin = BUZZER;
bool soundEnabled = true;
int inputButton = BUTTON;

// Secret mode requires 2 button presses.
// First press records the score, second press does the countdown.
bool secretModeEnabled = false;
int secretScore = 0;
bool secretScoreEntered = false;

// Digits to be displayed on 7SD.
int displayBuffer[2];

// Digit enable flags.
bool showLSD = true;
bool showMSD = true;
bool showDot = false;

// Digits queued for display after previous action finished, e.g. countdown timer.
int displayQueue[2];
bool showLSDQueue = false;
bool showMSDQueue = false;
bool showDotQueue = false;

// Record points entry, history.
int currentPoints = 0;
int entryIndex = 0; 
int pointsHistory[POINTS_HISTORY_MAX];

// 3...2...1... with beeping
bool doCountdown = false;
int countdownTimer = 3;
int countdownSpeedMs = 500;

// Countdown from max to 0.
int longCountdownTimer = 99;
bool doLongCountdown = false;
int longCountdownSpeedMs = 100;

// For blink action.
bool doBlink = false;
int nextBlinkState = 0;

// For custom delay operations that don't interrupt our 7-segment display refreshes.
unsigned long waitUntilMs = 0;

byte bcdCode[10][4] = {
//  D  C  B  A
  { 0, 0, 0, 0},  // 0
  { 0, 0, 0, 1},  // 1
  { 0, 0, 1, 0},  // 2
  { 0, 0, 1, 1},  // 3
  { 0, 1, 0, 0},  // 4
  { 0, 1, 0, 1},  // 5
  { 0, 1, 1, 0},  // 6
  { 0, 1, 1, 1},  // 7
  { 1, 0, 0, 0},  // 8
  { 1, 0, 0, 1}   // 9
};


//twinkle twinkle little star
int notesLength = 15;
char notes[] = "ccggaag ffeeddc ggffeed ggffeed ccggaag ffeeddc "; // a space represents a rest
int beats[] = { 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2 };
int tempo = 200;

void setup() {

  Serial.begin(9600);

  // Initialize display enable/disable pins
  for (int i=0; i < 2; i++)
  {
    pinMode(displayPins[i], OUTPUT);
  }

  // Initialize BCD-to7-segment driver
  for (int i=0; i < 4; i++)
  {
    pinMode(bcdPins[i], OUTPUT);
  }

  // Initialize dot for leftmost 7-segment display
  pinMode(decimalPoint, OUTPUT);

  // Input button for recording entry, starting countdown.
  pinMode(inputButton, INPUT);

  // Initialize dip switch points entry.
  for (int i=0; i < 4; i++)
  {
    pinMode(dipPins[i], INPUT_PULLUP);
  }

  // For blink (entry=14)
  pinMode(LED_BUILTIN, OUTPUT);

  randomSeed(analogRead(ANALOG_PIN_FOR_RAND_SEED));
}

void loop()
{
  if (doCountdown) {
    doCountdownLoop();
  }

  if (doLongCountdown) {
    doLongCountdownLoop();
  }

  if (doBlink) {
    doBlinkLoop();
  }

  int buttonState = digitalRead(inputButton);
  if (buttonState == LOW) {
    clearAllLoops();
    flushDisplayBuffers();
    refreshDisplay();
    delay(250);
    doButtonAction();
  }

  // Default, show display.
  refreshDisplay();
}

void playTone(int tone, int duration) {
  for (long i = 0; i < duration * 1000L; i += tone * 2) {
    digitalWrite(buzzerPin, HIGH);
    delayMicroseconds(tone);
    digitalWrite(buzzerPin, LOW);
    delayMicroseconds(tone);
  }
}

void playNote(char note, int duration) {
  char names[] = { 'c', 'd', 'e', 'f', 'g', 'a', 'b', 'C' };
  int tones[] = { 1915, 1700, 1519, 1432, 1275, 1136, 1014, 956 };
  
  // play the tone corresponding to the note name
  for (int i = 0; i < 8; i++) {
    if (names[i] == note) {
      playTone(tones[i], duration);
    }
  }
}

void playTwinkleTwinkleLittleStar() {
  if (!soundEnabled) {
    return;
  }
  
  for (int i = 0; i < notesLength; i++) {
    if (notes[i] == ' ') {
      delay(beats[i] * tempo); // rest
    } else {
      playNote(notes[i], beats[i] * tempo);
    }
    
    // pause between notes
    delay(tempo / 2); 
  }
}

void flushDisplayBuffers() 
{
  displayBuffer[0] = 0;
  displayBuffer[1] = 0;
  
  displayQueue[0] = 0;
  displayQueue[1] = 0;

  showMSD = false;
  showLSD = false;
  showDot = false;

  showLSDQueue = false;
  showMSDQueue = false;
  showDotQueue = false;
}

void displayDigit(int digit)
{
  for (int i=0; i < 4; i++)
  {
    digitalWrite(bcdPins[i], bcdCode[digit][i]);
  }
}

void refreshDisplay() {
  if (showLSD) {
    // Update LSD.
    digitalWrite(displayPins[1], LOW);
    digitalWrite(displayPins[0], HIGH);
    displayDigit(displayBuffer[0]);
    delay(5);
  } else {
    digitalWrite(displayPins[0], LOW);
  }

  if (showMSD) {
    // Update MSD.
    digitalWrite(displayPins[0], LOW);
    digitalWrite(displayPins[1], HIGH);
    displayDigit(displayBuffer[1]);
    delay(5);
  } else {
    digitalWrite(displayPins[1], LOW);
  }

  if (showDot) {
    displayDot();
  } else {
    hideDot();
  }
}

void disableDisplay() {
  digitalWrite(displayPins[0], LOW);
  digitalWrite(displayPins[1], LOW);
}

void displayDot() {
  digitalWrite(decimalPoint,LOW);
}

void hideDot() {
  digitalWrite(decimalPoint,HIGH);
}

void beep() {
  if (soundEnabled == true) {
    tone(buzzerPin, 1000);
  }
}

void noBeep() {
  if (soundEnabled == true) {
    noTone(buzzerPin);
  }
}

void wait(unsigned long ms) {    
  waitUntilMs = millis() + ms;  
}

bool waiting() {
  return millis() < waitUntilMs;
}

int calculateScoreFromDipEntry()
{
  int score = 0;
  
  if (digitalRead(dipPins[0]) == HIGH)
  {
    score += 1;
  }

  if (digitalRead(dipPins[1]) == HIGH)
  {
    score += 2;
  }

  if (digitalRead(dipPins[2]) == HIGH)
  {
    score += 4;
  }

  if (digitalRead(dipPins[3]) == HIGH)
  {
    score += 8;
  }

    return score;
}

void doCountdownLoop()
{
  showLSD = false;
  showMSD = true;
  showDot = false;
  
  if (countdownTimer < 0) {
    countdownTimer = 3;
    doCountdown = false;

    // Force long beep.
    delay(200); 
        
    // Shift queue to display.
    shiftQueueToDisplay();
  }
    
  if (waiting())
  {
    noBeep();
  }
  else
  {
    beep();
    displayBuffer[1] = countdownTimer--;
    wait(countdownSpeedMs);
  }
}

void doLongCountdownLoop() {
  showLSD = true;
  showMSD = true;
  showDot = false;

  if (longCountdownTimer < 0) {
    beep();
    showLSD = false;
    showMSD = false;
    refreshDisplay();
    delay(1500);
    noBeep();
    doLongCountdown = false;
  }

  if (!waiting())
  {
    intToDisplayBufferDigits(longCountdownTimer);
    longCountdownTimer--;
    wait(longCountdownSpeedMs);
  }
}

void intToDisplayBufferDigits(int num)
{
  displayBuffer[0] = num %10;
  displayBuffer[1] = num / 10;
}

void intToDisplayQueueDigits(int num)
{
  displayQueue[0] = num %10;
  displayQueue[1] = num / 10;
}

void shiftQueueToDisplay() {
    displayBuffer[0] = displayQueue[0];
    displayBuffer[1] = displayQueue[1];
    showLSD = showLSDQueue;
    showMSD = showMSDQueue;
    showDot = showDotQueue;
}

void doDisplay(int num) {
  switch (num) {
    case 1:
       displayQueue[0] = 1;
       displayQueue[1] = 0;
       showDotQueue = false;
       showLSDQueue = true;
       showMSDQueue = false;
       break;
    case 2:
       displayQueue[0] = 2;
       displayQueue[1] = 0;
       showDotQueue = false;
       showLSDQueue = true;
       showMSDQueue = false;
       break;
    case 3:
      displayQueue[0] = 3;
      displayQueue[1] = 0;
      showDotQueue = false;
      showLSDQueue = true;
      showMSDQueue = false;
      break;
    case 5:
      displayQueue[0] = 5;
      displayQueue[1] = 0;
      showDotQueue = false;
      showLSDQueue = true;
      showMSDQueue = false;
      break;
    case 8:
      displayQueue[0] = 8;
      displayQueue[1] = 0;
      showDotQueue = false;
      showLSDQueue = true;
      showMSDQueue = false;
      break;
    case 13:
      displayQueue[0] = 3;
      displayQueue[1] = 1;
      showDotQueue = false;
      showLSDQueue = true;
      showMSDQueue = true;
    break;

    // Half point == 15.
    case 15:
      displayQueue[0] = 5;
      displayQueue[1] = 0;      
      showDotQueue = true;
      showLSDQueue = true;
      showMSDQueue = true;
      break;
  }  
}

bool isPokerPointsScore(int num)
{
  switch (num) {
    case 1:
    case 2:
    case 3:
    case 5:
    case 8:
    case 13:
    case 15: // Half point.
      return true;
  }

  return false;
}

void doPokerPointsScoreAction(int score) {
  
  bool isValidScore = false;
  
  switch (score)
  {
    case 1:
       doDisplay(1);
       isValidScore = true;
    break;
    
    case 2:
       doDisplay(2);
       isValidScore = true;
    break;
    
    case 3:
      doDisplay(3);
      isValidScore = true;
    break;
    
    case 5:
      doDisplay(5);
      isValidScore = true;
    break;
    
    case 8:
      doDisplay(8);
      isValidScore = true;
    break;
    
    case 13:
      doDisplay(13);
      isValidScore = true;
    break;

    // Half point == 15.
    case 15:
      doDisplay(15);
      isValidScore = true;
    break;
  }

  if (isValidScore)
  {
    clearAllLoops();
    
    // Record the points.
    logPoints(score);

    // Ready countdown.
    doCountdown = true;
  }
}
  

void doButtonAction()
{
  int entry = calculateScoreFromDipEntry();

  // Points entry, or second button press after secret points entry.
  if (secretScoreEntered || isPokerPointsScore(entry)) {
    int score = -1;
    
    if (secretModeEnabled) {
      // First button press, record the score secretly.
      if (secretScoreEntered == false)
      {
        secretScore = entry;   
        secretScoreEntered = true; 
      } else {
        // Second button press, get the score from secret.
        score = secretScore;
        secretScore = -1;
        secretScoreEntered = false;
      }
    } else {
      score = entry; 
    }
    
    doPokerPointsScoreAction(score);
  } else {
    doSpecialEntryAction(entry);
  }
}

void doSpecialEntryAction(int num) {
  
  switch (num) {
    // Do countdown from max to 0.
    case 0:
      longCountdownTimer = 99;
      doLongCountdown = true;
    break;   

    // Play a song.  
    case 4:
      clearAllLoops();
      playTwinkleTwinkleLittleStar();
    break;

    // Display and print points history to log.  
    case 6:
      printPointsHistoryToTTY();
      
      {// !Block scoping prevents breaking subsequent switch cases.
        int i = 0;
        while (i < entryIndex) {
          if (!waiting()) {
            doDisplay(pointsHistory[i]);
            shiftQueueToDisplay();
            wait(500);
            i++;
          }
  
          refreshDisplay();
        }
      }
      
    break;

    // Toggle sound output  
    case 7:
      soundEnabled = !soundEnabled; 
      if (soundEnabled) {
        beep();
        delay(100);
        noBeep();
      }
    break;

    // Toggle secret score entry mode.  
    case 9:
      secretModeEnabled = !secretModeEnabled;
      beep();
      delay(100);
      noBeep();
    break;

    // Roll 1d6
    case 10:
      doDiceRoll(1, 6);      
    break;

    // Roll 1d20
    case 11:
      doDiceRoll(1, 20);
    break;

    // Roll 1d100
    case 12:
      doDiceRoll(1, 100);
    break;

    // Classic arduino blink routine.
    case 14:
      doBlink = true;
    break;
  }
}

void doDiceRoll(int nMin, int nMax) {
  long rnum = random(nMin, nMax + 1);
  intToDisplayQueueDigits(int(rnum));
  showLSDQueue = true;
  showMSDQueue = true;
  shiftQueueToDisplay();
  refreshDisplay();
}

void doBlinkLoop() {
  if (!waiting()) {
    switch (nextBlinkState) {
      case 0:
        digitalWrite(LED_BUILTIN, LOW);
        nextBlinkState = 1;
      break;
      case 1:
        digitalWrite(LED_BUILTIN, HIGH);
        nextBlinkState = 0;
      break;
    }

    wait(500);
  }
}

void logPoints(int score) {
  // Overflow.
  if (entryIndex >= POINTS_HISTORY_MAX) {
    entryIndex = 0;
  }
  
  pointsHistory[entryIndex] = score;
  entryIndex++;  
}

void printPointsHistoryToTTY() {
  for (int i = 0; i < entryIndex; i++)
  {
    Serial.println(intToPoints(pointsHistory[i]));
  }
}

float intToPoints(int num) {
  switch (num) {
    case 15:
      return 0.5f;
    default:
      return num;
  }
}

void clearAllLoops() {
  doBlink = false;
  doLongCountdown = false;
  doCountdown = false;
}
