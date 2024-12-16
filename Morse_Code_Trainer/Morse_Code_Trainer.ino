#include <U8g2lib.h>
#include <U8x8lib.h>
#include <Wire.h>

#define BUTTON_PIN 2
#define BUTTON_PIN_2 5
#define BUTTON_PIN_3 6
#define BUZZER_PIN 3
#define LED_PIN 4

// variables for button states
int buttonState = HIGH;
int lastButtonState = HIGH;
int buttonState2 = LOW;
int lastButtonState2 = LOW;
int buttonState3 = LOW;
int lastButtonState3 = LOW;
unsigned long buttonReleaseTime = 0;
unsigned long buttonPressTime = 0;
int howToUsePage = 0; // page tracker

// Timing constants
const unsigned long DOT_DURATION = 250;      // max duration for a dot (in ms)
const unsigned long CHAR_GAP_DURATION = 500; // gap duration indicating end of a character

bool isLongPress = false;
bool decoderOpen = false;
bool running = false;

unsigned long pressStartTime = 0;
unsigned long pressDuration = 0;
unsigned long lastActionTime = 0;   // time of the last button event
unsigned long stopwatchStartTime = 0;

String morseSequence = "";
String message = "";
unsigned long totalDots = 0;
float currentWPM = 0;

String userInputMorseLetter = ""; // for letter mode
String userInputMorseWord = "";   // for word mode

int chosenIndex = 99;
String chosenLetter = "";
const char* morseLetters[] = {
  ".-",    // A
  "-...",  // B
  "-.-.",  // C
  "-..",   // D
  ".",     // E
  "..-.",  // F
  "--.",   // G
  "....",  // H
  "..",    // I
  ".---",  // J
  "-.-",   // K
  ".-..",  // L
  "--",    // M
  "-.",    // N
  "---",   // O
  ".--.",  // P
  "--.-",  // Q
  ".-.",   // R
  "...",   // S
  "-",     // T
  "..-",   // U
  "...-",  // V
  ".--",   // W
  "-..-",  // X
  "-.--",  // Y
  "--..",  // Z
};

String userInputMorseNumber = ""; // for number mode
String chosenNumber = "";
const char* morseNumbers[] = {
  "-----",  // 0
  ".----",  // 1
  "..---",  // 2
  "...--",  // 3
  "....-",  // 4
  ".....",  // 5
  "-....",  // 6
  "--...",  // 7
  "---..",  // 8
  "----."   // 9
};

String chosenWord = "";  // new variable for the word mode
String practiceMorseSequence = "";
int menuIndex = 0;
const int menuItems = 3;  
int practiceMenuIndex = 0;
const int practiceMenuItems = 3;

enum MenuState {
  MAIN_MENU,
  DECODER_MENU,
  HOW_TO_USE_MENU,
  PRACTICE_MENU,
  DECODER_PRACTICE_MENU,
};

MenuState currentMenu = MAIN_MENU;

String selectedPracticeMode = "";


U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/ 10, /* reset=*/ 8);


void setup() {
  Serial.begin(9600);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setColorIndex(1);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(BUTTON_PIN_2, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  randomSeed(analogRead(0));
}

void loop() {
  // Handle the active menu state
  if (currentMenu == HOW_TO_USE_MENU) {
    readButton2();
    readHowToUseNavigationButton(); // navigate between pages
    displayHowToUse();             // show the current page
    return; // exit loop to prevent other menu actions
  }

  // other menu handling
  if (!decoderOpen) {
    readButton();
    readButton2();
    readButton3();
  } else {
    if (!running) {
      checkForStart();
    } else {
      morseCode();
    }
    readButton2();
    readButton3();
  }

  // handle practice input and validation
  if (currentMenu == DECODER_PRACTICE_MENU) {
    if (selectedPracticeMode == "LETTER") {
      morsePracticeInputLetter();
    } else if (selectedPracticeMode == "WORD") {
      morsePracticeInputWord();
    } else if (selectedPracticeMode == "NUMBER") {
      morsePracticeInputNumber();
    }
  }

  // refresh the screen
  refreshScreen();
}


void readButton() {
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == HIGH && lastButtonState == LOW) {
    buttonReleaseTime = millis();
    isLongPress = false;
    digitalWrite(LED_PIN, LOW);
  }

  if (buttonState == HIGH && (millis() - buttonReleaseTime) > 300) {
    isLongPress = true;
  }

  if (buttonState == LOW && lastButtonState == HIGH) {
    buttonPressTime = millis();
    unsigned long pressD = buttonPressTime - buttonReleaseTime;

    if (currentMenu == MAIN_MENU) {
      handleMainMenuInput(pressD);
    } else if (currentMenu == PRACTICE_MENU) {
      handlePracticeMenuInput(pressD);
    } 
  }
  lastButtonState = buttonState;
}

void readButton2() {
  buttonState2 = digitalRead(BUTTON_PIN_2);
  if (buttonState2 == HIGH && lastButtonState2 == LOW) {
    if (currentMenu == DECODER_MENU) {
      currentMenu = MAIN_MENU;
      menuIndex = 0;
      decoderOpen = false;
      running = false;
      morseSequence = ""; // Ensure morseSequence is cleared
      message = "";
    } else if (currentMenu == PRACTICE_MENU) {
      // Navigate back to the main menu from the practice menu
      currentMenu = MAIN_MENU;
      menuIndex = 0;
    } else if (currentMenu == DECODER_PRACTICE_MENU) {
      if (selectedPracticeMode == "LETTER" || 
          selectedPracticeMode == "WORD" || 
          selectedPracticeMode == "NUMBER") {
        // Clear all user input and temporary data
        message = "";
        userInputMorseLetter = "";
        userInputMorseWord = "";
        morseSequence = ""; // Clear morseSequence

        currentMenu = MAIN_MENU;
        menuIndex = 0;
        practiceMenuIndex = 0;
      } else {
        // Default behavior to go back to the main menu
        currentMenu = MAIN_MENU;
        menuIndex = 0;
        morseSequence = ""; // Ensure morseSequence is cleared here as well
      }
    } else if (currentMenu == HOW_TO_USE_MENU) {
      // Back button functionality in How to Use menu
      currentMenu = MAIN_MENU;
      menuIndex = 0;
    }
  }
  lastButtonState2 = buttonState2;
}


void handleMainMenuInput(unsigned long pressDuration) {
  if (isLongPress) {
    selectMenuItem(menuIndex);
  } else if (pressDuration < 300) {
    menuIndex = (menuIndex + 1) % menuItems;
  }
}

void handlePracticeMenuInput(unsigned long pressDuration) {
  if (isLongPress) {
    selectPracticeOption(practiceMenuIndex);
  } else if (pressDuration < 300) {
    practiceMenuIndex = (practiceMenuIndex + 1) % practiceMenuItems;
  }
}

void drawMenu() {
  if (currentMenu == MAIN_MENU) {
    drawMainMenu();
  } else if (currentMenu == PRACTICE_MENU) {
    drawPracticeMenu();
  } else if (currentMenu == DECODER_PRACTICE_MENU) {
    drawDecoderPracticeMenuLetter();
  }
}

// void drawDecoderIcon(int x, int y) {
//   // Draw outer rectangle
//   u8g2.drawFrame(x, y, 16, 16);

//   // Draw inner elements 
//   u8g2.drawBox(x + 7, y + 3, 6, 3);
//   u8g2.drawBox(x + 7, y + 7, 6, 3);
//   u8g2.drawBox(x + 7, y + 11, 6, 3);

//   u8g2.drawPixel(x + 4, y + 4);              
//   u8g2.drawPixel(x + 4, y + 8);  
//   u8g2.drawPixel(x + 4, y + 12);           
// }

void drawPracticeIcon(int x, int y) {
  u8g2.drawCircle(x, y, 8);
  u8g2.drawCircle(x, y, 6);  
  u8g2.drawLine(x - 3, y - 1, x - 1, y + 2);       
  u8g2.drawLine(x - 1, y + 2, x + 3, y - 2);
}

void drawMainMenu() {
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawFrame(0, 0, 128, 20);
  // drawDecoderIcon(10, 2);
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(12, 14, "SOS");
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawFrame(0, 22, 128, 20);
  drawPracticeIcon(18, 32);
  u8g2.drawFrame(0, 44, 128, 20);
  u8g2.drawStr(16, 58, "?");

  switch (menuIndex) {
    case 0:
      u8g2.drawBox(0, 0, 128, 20);
      u8g2.setDrawColor(0);
      u8g2.drawStr(52, 14, "Decoder");
      u8g2.setFont(u8g2_font_5x8_tf);
      u8g2.drawStr(12, 14, "SOS");
      u8g2.setFont(u8g2_font_6x13_tf);
      // drawDecoderIcon(10, 2);
      u8g2.setDrawColor(1);
      u8g2.drawStr(52, 36, "Practice");
      u8g2.drawStr(52, 58, "How to Use");
      break;
    case 1:
      u8g2.drawBox(0, 22, 128, 20);
      u8g2.drawStr(52, 14, "Decoder");
      u8g2.setDrawColor(0);
      u8g2.drawStr(52, 36, "Practice");
      drawPracticeIcon(18, 32);
      u8g2.setDrawColor(1);
      u8g2.drawStr(52, 58, "How to Use");
      break;
    case 2:
      u8g2.drawBox(0, 44, 128, 20);
      u8g2.drawStr(52, 14, "Decoder");
      u8g2.drawStr(52, 36, "Practice");
      u8g2.setDrawColor(0);
      u8g2.drawStr(52, 58, "How to Use");
      u8g2.drawStr(16, 58, "?");
      u8g2.setDrawColor(1);
      break;
  }
}

void drawDecoderMenu() {
  const char* text = "Press to START";
  u8g2.setFont(u8g2_font_6x10_tf);

  int x = (128 - u8g2.getStrWidth(text)) / 2;
  u8g2.drawStr(x, 32, text);
}

void drawPracticeMenu() {
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawFrame(0, 0, 128, 20);
  u8g2.drawStr(10, 14, "->");
  u8g2.drawFrame(0, 22, 128, 20);
  u8g2.drawStr(10, 36, "->");
  u8g2.drawFrame(0, 44, 128, 20);
  u8g2.drawStr(10, 58, "->");

  switch (practiceMenuIndex) {
    case 0:
      u8g2.drawBox(0, 0, 128, 20);
      u8g2.setDrawColor(0);
      u8g2.drawStr(52, 14, "Letter");
      u8g2.drawStr(10, 14, "->");
      u8g2.setDrawColor(1);
      u8g2.drawStr(52, 36, "Word");
      u8g2.drawStr(52, 58, "Number");
      break;
    case 1:
      u8g2.drawBox(0, 22, 128, 20);
      u8g2.drawStr(52, 14, "Letter");
      u8g2.setDrawColor(0);
      u8g2.drawStr(52, 36, "Word");
      u8g2.drawStr(10, 36, "->");
      u8g2.setDrawColor(1);
      u8g2.drawStr(52, 58, "Number");
      break;
    case 2:
      u8g2.drawBox(0, 44, 128, 20);
      u8g2.drawStr(52, 14, "Letter");
      u8g2.drawStr(52, 36, "Word");
      u8g2.setDrawColor(0);
      u8g2.drawStr(52, 58, "Number");
      u8g2.drawStr(10, 58, "->");
      u8g2.setDrawColor(1);
      break;
  }
}

void drawHowToUseDecoder() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(10, 10, "<< DECODER MODE >>");
    u8g2.drawStr(10, 24, "Red Btn: Type Morse Code");
    u8g2.drawStr(10, 38, "Blue Btn: Back");
    u8g2.drawStr(10, 52, "White Btn: Space Bar");
  } while (u8g2.nextPage());
}

void drawHowToUsePractice() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(10, 10, "<< PRACTICE MODE >>");
    u8g2.drawStr(10, 21, "Red Btn: Type Morse Code");
    u8g2.drawStr(15, 29, "- 1 Sec Press: Clear");
    u8g2.drawStr(10, 40, "Blue Btn: Back");
    u8g2.drawStr(10, 51, "White Btn: Submit");
    u8g2.drawStr(15, 58, "- Long Press: Shuffle");
  } while (u8g2.nextPage());
}

void displayHowToUse() {
  if (howToUsePage == 0) {
    drawHowToUseDecoder();
  } else if (howToUsePage == 1) {
    drawHowToUsePractice();
  }
}

// Add button logic to switch pages:
void readHowToUseNavigationButton() {
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == HIGH && lastButtonState == LOW) {
    howToUsePage = (howToUsePage + 1) % 2; // Toggle between page 0 and 1
  }
  lastButtonState = buttonState;
}

void drawDecoderPracticeMenuLetter() {
  u8g2.setFont(u8g2_font_6x10_tf);

  String label = "Letter: " + chosenLetter;
  u8g2.drawStr(6, 12, label.c_str()); 

  String userInput = "Input: " + userInputMorseLetter;
  u8g2.drawStr(6, 24, userInput.c_str());
  u8g2.drawStr(6, 36, message.c_str());
}

void drawDecoderPracticeMenuWord() {
  u8g2.setFont(u8g2_font_6x10_tf);

  // Display the label and the randomized word
  String label = "Word: " + chosenWord;
  u8g2.drawStr(6, 12, label.c_str());

  // Show the user's decoded Morse input so far
  String morseInput = "Morse: " + practiceMorseSequence;  
  u8g2.drawStr(6, 24, morseInput.c_str());

  // Display the currently typed Morse code for the current character
  String userInput = "Input: " + userInputMorseWord;
  u8g2.drawStr(6, 36, userInput.c_str());

  // Show feedback for correctness, split into two lines if too long
  int maxCharsPerLine = 20; // Approx. limit for one line
  if (message.length() <= maxCharsPerLine) {
    u8g2.drawStr(6, 48, message.c_str());
  } else {
    String line1 = message.substring(0, maxCharsPerLine);
    String line2 = message.substring(maxCharsPerLine);
    u8g2.drawStr(6, 48, line1.c_str());
    u8g2.drawStr(6, 60, line2.c_str());
  }
}

void drawDecoderPracticeMenuNumber() {
  u8g2.setFont(u8g2_font_6x10_tf);

  String label = "Number: " + chosenNumber;
  u8g2.drawStr(6, 12, label.c_str());

  String userInput = "Input: " + userInputMorseNumber;
  u8g2.drawStr(6, 24, userInput.c_str());

  u8g2.drawStr(6, 36, message.c_str());
}

void morsePracticeInputLetter() {
  unsigned long currentTime = millis();
  int bState = digitalRead(BUTTON_PIN);

  static int lButtonState = HIGH;
  static unsigned long pStartTime = 0;

  if (lButtonState == LOW && bState == HIGH) {
    pStartTime = currentTime;
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    lastActionTime = currentTime;
  }

  if (lButtonState == HIGH && bState == LOW) {
    unsigned long pDuration = currentTime - pStartTime;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    if (pDuration > 500) { 
      userInputMorseLetter = ""; 
      message = "";
      if (userInputMorseLetter.length() != 0) {
        checkWordInput();
      }
      userInputMorseLetter = ""; 
      message = "";        
    } else { 
      if (pDuration < DOT_DURATION) {
        userInputMorseLetter += ".";
      } else {
        userInputMorseLetter += "-";
      }
    }
    lastActionTime = currentTime;
  }

  lButtonState = bState;
}

static String tempMorseSequence = ""; // Temporary storage for current character

void morsePracticeInputWord() {
  unsigned long currentTime = millis();
  int bState = digitalRead(BUTTON_PIN);

  static int lButtonState = HIGH;
  static unsigned long pStartTime = 0;

  if (lButtonState == LOW && bState == HIGH) {
    pStartTime = currentTime;
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    lastActionTime = currentTime;
  }

  if (lButtonState == HIGH && bState == LOW) {
    unsigned long pDuration = currentTime - pStartTime;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    if (pDuration < DOT_DURATION) {
      tempMorseSequence += ".";
    } else {
      tempMorseSequence += "-";
    }

    practiceMorseSequence = tempMorseSequence;
    lastActionTime = currentTime;
  }

  if (tempMorseSequence.length() > 0 && ((currentTime - lastActionTime) > CHAR_GAP_DURATION)) {
    char decodedCharacterPractice = decodeMorse(tempMorseSequence);
    userInputMorseWord += decodedCharacterPractice;
    tempMorseSequence = "";
    practiceMorseSequence = "";
  }

  lButtonState = bState;
}


void morsePracticeInputNumber() {
  unsigned long currentTime = millis();
  int bState = digitalRead(BUTTON_PIN);

  static int lButtonState = HIGH;
  static unsigned long pStartTime = 0;

  if (lButtonState == LOW && bState == HIGH) {
    pStartTime = currentTime;
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    lastActionTime = currentTime;
  }

  if (lButtonState == HIGH && bState == LOW) {
    unsigned long pDuration = currentTime - pStartTime;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    if (pDuration > 500) { 
      userInputMorseNumber = ""; 
      message = "";
      if (userInputMorseNumber.length() != 0) {
        checkNumberInput();
      }
      userInputMorseNumber = ""; 
      message = "";
    } else { 
      if (pDuration < DOT_DURATION) {
        userInputMorseNumber += ".";
      } else {
        userInputMorseNumber += "-";
      }
    }
    lastActionTime = currentTime;
  }

  lButtonState = bState;
}

void checkNumberInput() {
  String correctNumberMorse = morseNumbers[chosenIndex];
  if (userInputMorseNumber == correctNumberMorse) {
    message = "Correct!";
  } else {
    message = "Wrong: " + correctNumberMorse;
  }
}

void updatePracticeScreen() {
  u8g2.firstPage();
  do {
    if (selectedPracticeMode == "LETTER") {
      drawDecoderPracticeMenuLetter(); 
    } else if (selectedPracticeMode == "WORD") {
      drawDecoderPracticeMenuWord();  
    }
  } while (u8g2.nextPage());
}


void drawDecoderRunningScreen() {
  u8g2.setFont(u8g2_font_ncenB14_tr);
  String wpmText = String(currentWPM, 1) + " WPM";
  int wpmX = (128 - u8g2.getStrWidth(wpmText.c_str())) / 2;
  u8g2.drawStr(wpmX, 16, wpmText.c_str());

  u8g2.setFont(u8g2_font_6x10_tf);
  String morseText = "Morse: " + morseSequence;
  u8g2.drawStr(0, 32, morseText.c_str());

  String messageText = "Message: " + message;
  if (messageText.length() > 20) {
    messageText = messageText.substring(messageText.length()-20);
  }
  u8g2.drawStr(0, 48, messageText.c_str());
}

void selectMenuItem(int index) {
  switch (index) {
    case 0:
      currentMenu = DECODER_MENU;
      decoderOpen = true;
      running = false;
      break;
    case 1:
      currentMenu = PRACTICE_MENU;
      practiceMenuIndex = 0;
      break;
    case 2:
      currentMenu = HOW_TO_USE_MENU;
      howToUsePage = 0;
      break;
  }
}

void selectPracticeOption(int index) {
  switch (index) {
    case 0:  // Letter mode
      selectedPracticeMode = "LETTER";
      pickRandomLetter();
      currentMenu = DECODER_PRACTICE_MENU;
      decoderOpen = false;
      break;
    case 1:  // Word mode
      selectedPracticeMode = "WORD";
      pickRandomWord();
      currentMenu = DECODER_PRACTICE_MENU;
      decoderOpen = false;
      break;
    case 2:  // Number mode
      selectedPracticeMode = "NUMBER";
      pickRandomNumber();
      currentMenu = DECODER_PRACTICE_MENU;
      decoderOpen = false;
      break;
    case 3:  // Back to main menu
      currentMenu = MAIN_MENU;
      menuIndex = 0;
      break;
  }
}


void checkForStart() {
  unsigned long currentTime = millis();
  int bState = digitalRead(BUTTON_PIN);

  static int lButtonState = HIGH;
  static unsigned long pStartTime = 0;

  if (lButtonState == HIGH && bState == LOW) {
    pStartTime = currentTime;
  }
  if (lButtonState == LOW && bState == HIGH) {
    unsigned long pDuration = currentTime - pStartTime;
    if (pDuration > DOT_DURATION) {
      running = true;
      stopwatchStartTime = millis();
      morseSequence = "";
      message = "";
      totalDots = 0;
      currentWPM = 0;
    }
  }
  lButtonState = bState;
}

void morseCode() {
  unsigned long currentTime = millis();
  int bState = digitalRead(BUTTON_PIN);

  static int lButtonState = HIGH;
  static unsigned long pStartTime = 0;

  if (lButtonState == LOW && bState == HIGH) {
    pStartTime = currentTime;
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    lastActionTime = currentTime;
  }
  if (lButtonState == HIGH && bState == LOW) {
    unsigned long pDuration = currentTime - pStartTime;
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);

    if (pDuration < DOT_DURATION) {
      morseSequence += ".";
      totalDots += 2;
    } else {
      morseSequence += "-";
      totalDots += 4;
    }
    countWPM(currentTime);
    lastActionTime = currentTime;
  }

  if (morseSequence.length() > 0 && ((currentTime - lastActionTime) > CHAR_GAP_DURATION)) {
    String decodedCharacter = decodeMorseSequence(morseSequence);
    message += decodedCharacter;
    morseSequence = "";
    totalDots += 3;
  }

  lButtonState = bState;
}

void countWPM(unsigned long currentTime) {
  static unsigned long lastCheckTime = 0;

  if (currentTime - lastCheckTime >= 1000) {
    if (stopwatchStartTime > 0) {
      unsigned long elapsedTime = currentTime - stopwatchStartTime;
      float minutes = elapsedTime / 60000.0;
      currentWPM = (totalDots / 50.0) / minutes; 
    }
    lastCheckTime = currentTime;
  }
}

char decodeMorse(String sequence) {
  if (sequence == ".-") return 'A';
  else if (sequence == "-...") return 'B';
  else if (sequence == "-.-.") return 'C';
  else if (sequence == "-..") return 'D';
  else if (sequence == ".") return 'E';
  else if (sequence == "..-.") return 'F';
  else if (sequence == "--.") return 'G';
  else if (sequence == "....") return 'H';
  else if (sequence == "..") return 'I';
  else if (sequence == ".---") return 'J';
  else if (sequence == "-.-") return 'K';
  else if (sequence == ".-..") return 'L';
  else if (sequence == "--") return 'M';
  else if (sequence == "-.") return 'N';
  else if (sequence == "---") return 'O';
  else if (sequence == ".--.") return 'P';
  else if (sequence == "--.-") return 'Q';
  else if (sequence == ".-.") return 'R';
  else if (sequence == "...") return 'S';
  else if (sequence == "-") return 'T';
  else if (sequence == "..-") return 'U';
  else if (sequence == "...-") return 'V';
  else if (sequence == ".--") return 'W';
  else if (sequence == "-..-") return 'X';
  else if (sequence == "-.--") return 'Y';
  else if (sequence == "--..") return 'Z';
  else if (sequence == "-----") return '0';
  else if (sequence == ".----") return '1';
  else if (sequence == "..---") return '2';
  else if (sequence == "...--") return '3';
  else if (sequence == "....-") return '4';
  else if (sequence == ".....") return '5';
  else if (sequence == "-....") return '6';
  else if (sequence == "--...") return '7';
  else if (sequence == "---..") return '8';
  else if (sequence == "----.") return '9';
  return '?';
}

String decodeMorseSequence(String sequence) {
  char decodedChar = decodeMorse(sequence);
  return String(decodedChar);
}

char indexToChar(int index) {
  if (index >= 0 && index <= 25) {
    return (char)('A' + index);
  } else if (index >= 26 && index <= 35) {
    return (char)('0' + (index - 26));
  }
  return '?'; 
}

void pickRandomLetter() {
  int randomIndex = random(0, 26);
  chosenIndex = randomIndex;
  chosenLetter = String(indexToChar(chosenIndex));
}

void pickRandomNumber() {
  int randomIndex = random(0, 10); 
  chosenIndex = randomIndex;
  chosenNumber = String(indexToChar(26 + chosenIndex)); 
}

void pickRandomWord() {
  String wordList[] = {"HELLO", "HELP", "YES", "NO", "SOS"};
  int wordCount = sizeof(wordList) / sizeof(wordList[0]);

  int randomWordIndex = random(0, wordCount);
  chosenWord = wordList[randomWordIndex]; 

  for (int i = 0; i < chosenWord.length(); i++) {
    if (chosenWord[i] < 'A' || chosenWord[i] > 'Z') {
      chosenWord[i] = ' ';
    }
  }
}


void checkWordInput() {
  String correctMorse = "";
  for (char c : chosenWord) {
    if (c >= 'A' && c <= 'Z') {
      correctMorse += morseLetters[c - 'A'];
    } else if (c >= '0' && c <= '9') {
      correctMorse += morseLetters[26 + (c - '0')];
    }
    correctMorse += " ";
  }
  correctMorse.trim(); 
}


String decodedWord = "";

String decodeMorseWord(String morseSequence) {
  String morseChar = "";
  for (char c : morseSequence) {
    if (c == ' ') {
      decodedWord += decodeMorse(morseChar);
      morseChar = "";
    } else {
      morseChar += c;
    }
  }
  if (morseChar.length() > 0) {
    decodedWord += decodeMorse(morseChar);
  }

  return decodedWord;
}

void refreshScreen() {
  u8g2.firstPage();
  do {
    if (currentMenu == MAIN_MENU) {
      drawMainMenu();
    } else if (currentMenu == PRACTICE_MENU) {
      drawPracticeMenu();
    } else if (currentMenu == DECODER_MENU) {
      if (!running) {
        drawDecoderMenu(); 
      } else {
        drawDecoderRunningScreen();
      }
    } else if (currentMenu == DECODER_PRACTICE_MENU) {
      if (selectedPracticeMode == "LETTER") {
        drawDecoderPracticeMenuLetter();
      } else if (selectedPracticeMode == "WORD") {
        drawDecoderPracticeMenuWord();
      } else if (selectedPracticeMode == "NUMBER") {
        drawDecoderPracticeMenuNumber();
      }
    }
  } while (u8g2.nextPage());
}

void readButton3() {
  buttonState3 = digitalRead(BUTTON_PIN_3);
  static unsigned long buttonPressStartTime = 0;

  if (buttonState3 == HIGH && lastButtonState3 == LOW) {
    buttonPressStartTime = millis();
  } else if (buttonState3 == LOW && lastButtonState3 == HIGH) {
    unsigned long pressDuration = millis() - buttonPressStartTime;

    if (pressDuration > 500) { 
      if (currentMenu == DECODER_PRACTICE_MENU) {
        if (selectedPracticeMode == "LETTER") {
          pickRandomLetter();
        } else if (selectedPracticeMode == "WORD") {
          pickRandomWord();
        } else if (selectedPracticeMode == "NUMBER") {
          pickRandomNumber();
        }
      }
    } else {
      if (currentMenu == DECODER_MENU) {
        message += " ";
        totalDots += 4;
      } else if (currentMenu == DECODER_PRACTICE_MENU) {
        if (selectedPracticeMode == "LETTER") {
          String correctLetterMorse = morseLetters[chosenIndex];
          if (userInputMorseLetter == correctLetterMorse) {
            message = "Correct!";
          } else {
            message = "Wrong: " + correctLetterMorse;
          }
          userInputMorseLetter = "";
        } else if (selectedPracticeMode == "WORD") {
          String translatedInput = userInputMorseWord;
          String normalizedChosenWord = chosenWord;
          normalizedChosenWord.toUpperCase();
          translatedInput.toUpperCase();

          String correctWordMorse = "";
          for (char c : chosenWord) {
            if (c >= 'A' && c <= 'Z') {
              correctWordMorse += morseLetters[c - 'A'];
            } else if (c >= '0' && c <= '9') {
              correctWordMorse += morseNumbers[c - '0'];
            }
            correctWordMorse += " ";
          }
          correctWordMorse.trim();

          if (translatedInput == normalizedChosenWord) {
            message = "Correct!";
          } else {
            message = "Wrong: " + correctWordMorse;
          }

          userInputMorseWord = "";
        } else if (selectedPracticeMode == "NUMBER") {
          String correctNumberMorse = morseNumbers[chosenIndex];
          if (userInputMorseNumber == correctNumberMorse) {
            message = "Correct!";
          } else {
            message = "Wrong: " + correctNumberMorse;
          }
          userInputMorseNumber = "";
        }
        lastActionTime = millis();
      }
    }
  }
  lastButtonState3 = buttonState3;
}
