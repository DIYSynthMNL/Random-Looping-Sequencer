/*
  Random Looping Sequencer V 0.2
  Code by: Neo Recasata

  enc pin | ard pin
  1         D2
  2         GND
  3         D3
  sw1       D4
  sw2       GND
*/

#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <SimpleRotary.h>
#include <LedDisplay.h>
#include <EEPROM.h>

#define EEPROM_INITIALIZED_MARKER 123
int markerAddress = 0;

#define DATA_PIN 6
#define REGISTER_SELECT 7
#define CLOCK_PIN 8
#define ENABLE 9
#define RESET 10
#define DISPLAY_LENGTH 8

#define clockIn 11
#define cvEraseIn 12

// Struct to represent a menu item
struct MenuItem {
  const char* label;
  int value;
  bool selected;
  bool editing;
  int min;
  int max;
  int increment;
};

// Array of menu items
MenuItem menuItems[] = {
      {"STPS:", 16, false, false, 1, 16, 1}, // number of steps
      {"CvP: ", 0, false, false, 0, 100, 10}, // cv probability
};

Adafruit_MCP4725 dac;
LedDisplay myDisplay = LedDisplay(DATA_PIN, REGISTER_SELECT, CLOCK_PIN,
                                  ENABLE, RESET, DISPLAY_LENGTH);
SimpleRotary rotary(2, 3, 4); 

int cvSequence[16];
int currentStep = 0;
int cvRandNum = 0;
int cvOutValue = 816;
bool stepChangedOnClockPulse = false;
// quantizing array
const int chromatic[] = {0, 68,  136, 204, 272, 340, 408, 476, 544, 612, 680, 748, 816, 884, 952, 1020,  1088,  1156,  1224,  1292,  1360,  1428,  1496,  1564,  1632,  1700,  1768,  1836,  1904,  1972,  2040 , 2108,  2176,  2244,  2312 , 2380,  2448,  2516,  2584,  2652 , 2720,  2788,  2856,  2924,  2992,  3060 , 3128,  3196,  3264,  3332,  3400 , 3468 , 3536 , 3604,  3672,  3740,  3808,  3876,  3944,  4012, 4080};
const int chromaticSize = sizeof(chromatic) / sizeof(chromatic[0]);

const int majorScale[] = {2, 2, 1, 2, 2, 2, 1};  // W-W-H-W-W-W-H
const int minorScale[] = {2, 1, 2, 2, 1, 2, 2};  // W-H-W-W-H-W-W
const int chromaticScale[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

int brightness = 15;
int mainMenuSelection = 0;
int blinkDuration = 200;
unsigned long lastBlinkTime = 0;
bool isBlinkVisible = true;
bool isEditing = false;

void setup() {
  Serial.begin(9600);

  if (isEEPROMInitialized() == true) {
    menuItems[0].value = EEPROM.read(1);
    menuItems[1].value = EEPROM.read(2);
  }

  myDisplay.begin();
  

  // For Adafruit MCP4725A1 the address is 0x62 (default) or 0x63 (ADDR pin tied to VCC)
  // For MCP4725A0 the address is 0x60 or 0x61
  // For MCP4725A2 the address is 0x64 or 0x65
  dac.begin(0x60);
  dac.setVoltage(816, false);
}

void loop() {
  myDisplay.setBrightness(brightness);
  handleMenu();

  if(currentStep < menuItems[0].value) {
    if (digitalRead(clockIn) == LOW && stepChangedOnClockPulse == false) {
      stepChangedOnClockPulse = true;
      changeStepCv();

      // Output cv
      dac.setVoltage(cvSequence[currentStep], false);
      
      Serial.print(currentStep);
      Serial.print(" | ");
      Serial.print(cvSequence[currentStep]);
      Serial.println("");

      currentStep += 1;
      
    }
    if (digitalRead(clockIn) == HIGH && stepChangedOnClockPulse == true) {
      stepChangedOnClockPulse = false;
    }
  }
  else {
    currentStep = 0;
  }
}

void changeStepCv() {
  // TODO change random number to select an index from the scale chosen
  // choose a random index
  cvRandNum = random(0, sizeof(chromatic)/sizeof(chromatic[0]));
  cvOutValue = chromatic[cvRandNum];

  // randomly change values in the array depending on the menu item value
  if (getRandomBool(menuItems[1].value)) {
    cvSequence[currentStep] = cvOutValue;
  }
}

bool getRandomBool(int probability) {
  // returns true or false based on the probability
  int randVal = random(1, 100);
  return randVal <= probability;
}

void handleMenu() {
  if (isEditing == false) {
    // initialize menu
    for (int i = 0; i < sizeof(menuItems) / sizeof(menuItems[0]); ++i) {
      if (mainMenuSelection == i && !menuItems[i].selected) {
        initializeMenu(menuItems[i]);
      }
    }
    rotateMainMenu();
  }

  byte rotaryButton = rotary.push();

  if (rotaryButton == 1 && isEditing == false) {
    isEditing = true;
  }
  else if (rotaryButton == 1 && isEditing == true) {
    for (int i = 0; i < sizeof(menuItems) / sizeof(menuItems[0]); ++i) {
      if(menuItems[i].selected) {
        myDisplay.setCursor(5);
        myDisplay.print("   ");
        myDisplay.setCursor(5);
        myDisplay.print(menuItems[i].value);
      }
    }
    storeValuesInMemory();
    isEditing = false;
  }

  if (isEditing == true) {
    for (int i = 0; i < sizeof(menuItems) / sizeof(menuItems[0]); ++i) {
      if (menuItems[i].selected) {
        byte rotaryDirection = rotary.rotate();
        if (rotaryDirection == 1 && menuItems[i].selected) {
          menuItems[i].value = min(menuItems[i].value + menuItems[i].increment, menuItems[i].max);
          updateDisplayOnBlink(menuItems[i].value);
        }
        if (rotaryDirection == 2 && menuItems[i].selected) {
          menuItems[i].value = max(menuItems[i].value - menuItems[i].increment, menuItems[i].min);
          updateDisplayOnBlink(menuItems[i].value);
        }
        if (rotaryDirection == 0) {
          blinkDisplay(menuItems[i].value);
        }
      }
    }
  }
}

void initializeMenu(MenuItem& menuItem) {
  menuItem.selected = true;
  myDisplay.clear();
  myDisplay.home();
  myDisplay.print(menuItem.label);
  myDisplay.print(menuItem.value);
}

void rotateMainMenu() {
  byte rotaryDirection = rotary.rotate();
  if (rotaryDirection == 1 && mainMenuSelection < sizeof(menuItems) / sizeof(menuItems[0]) - 1) {
    changeMenuSelection(1);
  }

  if (rotaryDirection == 2 && mainMenuSelection > 0) {
    changeMenuSelection(-1);
  }
}

void changeMenuSelection(int direction) {
  mainMenuSelection += direction;
  for (int i = 0; i < sizeof(menuItems) / sizeof(menuItems[0]); ++i) {
    menuItems[i].selected = false;
  }
}

void blinkDisplay(int value) {
  if (millis() - lastBlinkTime >= blinkDuration) {
    lastBlinkTime = millis();
    isBlinkVisible = !isBlinkVisible;
    updateDisplayOnBlink(value);
  }
}

void updateDisplayOnBlink(int value) {
  myDisplay.setCursor(5);
  myDisplay.print("   ");
  myDisplay.setCursor(5);
  if (isBlinkVisible) {
    myDisplay.print(value);
  }
}

void storeValuesInMemory() {
  EEPROM.write(markerAddress, EEPROM_INITIALIZED_MARKER);
  EEPROM.write(1, menuItems[0].value);
  EEPROM.write(2, menuItems[1].value);
}

bool isEEPROMInitialized() {
  int markerValue;
  markerValue = EEPROM.read(markerAddress);

  return (markerValue == EEPROM_INITIALIZED_MARKER);
}

void generateScale(int root, const int scaleType[], int scaleTypeSize, int noteRange, int scale[]) {
  // Ensure root is within bounds
  root = constrain(root, 0, chromaticSize - 1);

  // Ensure noteRange is within bounds
  noteRange = constrain(noteRange, 1, chromaticSize);

  // Calculate the starting index based on the root note
  int startIndex = root;

  // Generate the scale array based on scale type and note range
  for (int i = 0; i < noteRange; i++) {
    scale[i] = chromatic[startIndex];
    startIndex = (startIndex + scaleType[i % scaleTypeSize]) % chromaticSize;
  }
}
