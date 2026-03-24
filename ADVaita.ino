#include "M5Cardputer.h"

enum AppState { MAIN_MENU, RUNNING_PATTERN };

enum BreathingPhase {
  PHASE_STARTING,
  PHASE_BREATH_IN,
  PHASE_HOLD_FULL,
  PHASE_BREATH_OUT,
  PHASE_HOLD_EMPTY
};

enum PatternType {
  PATTERN_SQUARE,
  PATTERN_TRIANGLE,
  PATTERN_COMBO
};

struct PatternDef {
  int startSec = 5;
  int inSec;
  int holdFullSec;
  int outSec;
  int holdEmptySec;
  int totalSec;
  const char *name;
  bool hasHoldEmpty;
  PatternType type;
  const char *totalTimeStr;
};

PatternDef patternSquare = {5, 4, 4, 4, 4, 304, "Square", true, PATTERN_SQUARE, "5 mins 04 secs"};
PatternDef patternTriangle = {5, 4, 7, 8, 0, 361, "Triangle", false, PATTERN_TRIANGLE, "6 mins 01 secs"};
PatternDef patternCombo = {5, 4, 4, 4, 4, 665, "Combo", true, PATTERN_COMBO, "11 mins 05 secs"};

enum Theme { THEME_MONOCHROME, THEME_LED };
Theme currentTheme = THEME_MONOCHROME;
bool showHelp = false;

AppState currentState = MAIN_MENU;
PatternDef *activePattern = &patternSquare;

BreathingPhase currentPhase = PHASE_STARTING;
int phaseTicksRemaining = 0;
int phaseDuration = 0;
int totalSecondsRemaining = 0;
int totalDuration = 0;
unsigned long lastTickTime = 0;
bool isPaused = false;
bool soundEnabled = true;
bool screenOn = true;
int volumeLevel = 128;
int brightnessLevel = 128;
int barWidth = 0;

// v0.9: Inactivity tracking
unsigned long lastInteractionTime = 0;
bool autoDisplayOff = false;

// Theme Colors
uint16_t COLOR_BG_MONO = M5Cardputer.Display.color565(250, 250, 250);
uint16_t COLOR_TEXT = BLACK;
uint16_t COLOR_BAR_BG = M5Cardputer.Display.color565(180, 180, 180);
uint16_t COLOR_BAR_FG = BLACK;

// v0.8: Randomized LED Colors
uint16_t ledColorPool[10];
uint16_t currentLEDColor;

void initColorPool() {
  ledColorPool[0] = M5Cardputer.Display.color565(230, 200, 255); // Lavender
  ledColorPool[1] = M5Cardputer.Display.color565(200, 230, 255); // Sky Blue
  ledColorPool[2] = M5Cardputer.Display.color565(255, 200, 200); // Rose
  ledColorPool[3] = M5Cardputer.Display.color565(200, 255, 200); // Mint
  ledColorPool[4] = M5Cardputer.Display.color565(255, 230, 180); // Peach
  ledColorPool[5] = M5Cardputer.Display.color565(180, 255, 240); // Aqua
  ledColorPool[6] = M5Cardputer.Display.color565(255, 255, 200); // Lemon
  ledColorPool[7] = M5Cardputer.Display.color565(210, 180, 240); // Lilac (Purple Variant)
  ledColorPool[8] = M5Cardputer.Display.color565(220, 180, 255); // Mauve (Purple Variant)
  ledColorPool[9] = M5Cardputer.Display.color565(240, 210, 255); // Very Light Orchid (Purple Variant)
}

void pickRandomLEDColor() {
  currentLEDColor = ledColorPool[random(10)];
}

void setComboPattern() {
  patternCombo.inSec = 4;
  patternCombo.holdFullSec = 4;
  patternCombo.outSec = 4;
  patternCombo.holdEmptySec = 4;
  patternCombo.hasHoldEmpty = true;
  patternCombo.totalSec = 665;
}

M5Canvas canvas(&M5Cardputer.Display);

void drawScreen(); // Forward declaration (v0.7 optimization)

void playBeep(int freq, int duration = 15) {
  if (!soundEnabled) return;
  M5Cardputer.Speaker.tone(freq, duration);
}

void playPhaseBeep(BreathingPhase phase) {
  if (phase == PHASE_STARTING && currentTheme == THEME_MONOCHROME) return;
  
  if (currentTheme == THEME_MONOCHROME) {
    int freq = 1000;
    switch (phase) {
    case PHASE_BREATH_IN: freq = 1200; break;
    case PHASE_HOLD_FULL: freq = 800; break;
    case PHASE_BREATH_OUT: freq = 600; break;
    case PHASE_HOLD_EMPTY: freq = 800; break;
    }
    playBeep(freq, 20);
  } else {
    // LED Theme sounds from v0.1
    int freq = 440;
    switch (phase) {
    case PHASE_STARTING: freq = 440; break;
    case PHASE_BREATH_IN: freq = 660; break;
    case PHASE_HOLD_FULL: freq = 880; break;
    case PHASE_BREATH_OUT: freq = 330; break;
    case PHASE_HOLD_EMPTY: freq = 880; break;
    }
    playBeep(freq, 200); 
  }
}

void advancePhase() {
  if (activePattern->type == PATTERN_COMBO && totalSecondsRemaining == 361 && currentPhase == PHASE_HOLD_EMPTY) {
    activePattern->inSec = 4;
    activePattern->holdFullSec = 7;
    activePattern->outSec = 8;
    activePattern->hasHoldEmpty = false;
    
    currentPhase = PHASE_BREATH_IN;
    phaseTicksRemaining = activePattern->inSec;
    phaseDuration = phaseTicksRemaining;
    
    // Draw immediately to sync with the start of transition beeps (v0.7 optimization)
    drawScreen();
    
    playBeep(2000, 30);
    delay(40);
    playBeep(2000, 30);
    return;
  }

  switch (currentPhase) {
  case PHASE_STARTING:
    currentPhase = PHASE_BREATH_IN;
    phaseTicksRemaining = activePattern->inSec;
    break;
  case PHASE_BREATH_IN:
    currentPhase = PHASE_HOLD_FULL;
    phaseTicksRemaining = activePattern->holdFullSec;
    break;
  case PHASE_HOLD_FULL:
    currentPhase = PHASE_BREATH_OUT;
    phaseTicksRemaining = activePattern->outSec;
    break;
  case PHASE_BREATH_OUT:
    if (activePattern->hasHoldEmpty) {
      currentPhase = PHASE_HOLD_EMPTY;
      phaseTicksRemaining = activePattern->holdEmptySec;
    } else {
      currentPhase = PHASE_BREATH_IN;
      phaseTicksRemaining = activePattern->inSec;
    }
    break;
  case PHASE_HOLD_EMPTY:
    currentPhase = PHASE_BREATH_IN;
    phaseTicksRemaining = activePattern->inSec;
    break;
  }
  phaseDuration = phaseTicksRemaining;

  // Immediate redraw for LED theme to eliminate stutter (v0.7 optimization)
  if (currentTheme == THEME_LED) {
    drawScreen();
  }

  playPhaseBeep(currentPhase);
}

void startPattern() {
  currentState = RUNNING_PATTERN;
  currentPhase = PHASE_STARTING;
  phaseTicksRemaining = activePattern->startSec;
  phaseDuration = activePattern->startSec;
  totalDuration = activePattern->totalSec;
  totalSecondsRemaining = totalDuration;
  isPaused = false;
  lastTickTime = millis();
}

void drawEnso(int x, int y, int r) {
  // Draw a brush-like Enso circle using several arcs
  for (float i = 0; i < 3; i+=0.5) {
    canvas.drawArc(x, y, r-i, r-i-1, 30, 330, COLOR_BAR_BG);
  }
  // Thicker part at the beginning of the stroke
  canvas.fillCircle(x + r*cos(30*PI/180.0), y + r*sin(30*PI/180.0), 3, COLOR_BAR_BG);
}

void drawHelpOverlay() {
  canvas.fillRect(20, 15, 200, 105, M5Cardputer.Display.color565(240, 240, 240));
  canvas.drawRect(20, 15, 200, 105, BLACK);
  canvas.setTextColor(BLACK);
  canvas.setTextSize(1);
  canvas.setTextDatum(top_left);
  int startX = 35;
  int startY = 25;
  int lineH = 12;
  canvas.drawString("esc: exit to menu", startX, startY);
  canvas.drawString("space: pause", startX, startY + lineH);
  canvas.drawString("a: switch theme", startX, startY + 2*lineH);
  canvas.drawString("s: sound on/off", startX, startY + 3*lineH);
  canvas.drawString("d: display on/off", startX, startY + 4*lineH);
  canvas.drawString("-+: sound level", startX, startY + 5*lineH);
  canvas.drawString(".;: display brightness level", startX, startY + 6*lineH);
  canvas.drawString("h: help (this screen)", startX, startY + 7*lineH);
}

void drawScreen() {
  uint16_t bgColor = COLOR_BG_MONO;
  if (currentTheme == THEME_LED && currentState == RUNNING_PATTERN) {
    bgColor = currentLEDColor; // v0.8: Fixed color during pattern, randomized on toggle
  }

  canvas.fillScreen(bgColor);
  canvas.setTextColor(COLOR_TEXT);
  canvas.setTextDatum(top_left);

  if (currentState == MAIN_MENU) {
    // Logo and Title
    drawEnso(70, 27, 18);
    canvas.setTextColor(BLACK);
    canvas.setTextSize(2);
    canvas.drawString("ADVaita", 95, 20);
    
    // Patterns
    canvas.setTextSize(3);
    canvas.drawString("1.Square", 20, 55);
    canvas.drawString("2.Triangle", 20, 80);
    canvas.drawString("3.Combo", 20, 105);
    
    // Help tip
    canvas.setTextSize(1);
    canvas.setTextDatum(bottom_right);
    canvas.drawString("h: help", 235, 130);

  } else if (currentState == RUNNING_PATTERN) {
    if (currentTheme == THEME_MONOCHROME) {
      if (currentPhase == PHASE_STARTING) {
        canvas.setTextDatum(middle_center);
        canvas.setTextSize(6);
        canvas.drawString(String(phaseTicksRemaining), 120, 67);
      } else {
        canvas.setTextDatum(middle_center);
        canvas.setTextSize(5);
        if (currentPhase == PHASE_BREATH_IN) {
          canvas.drawString("in", 120, 55);
          barWidth = 60;
        } else if (currentPhase == PHASE_BREATH_OUT) {
          canvas.drawString("out", 120, 55);
          barWidth = 90;
        } else if (currentPhase == PHASE_HOLD_FULL || currentPhase == PHASE_HOLD_EMPTY) {
          canvas.drawString("hold", 120, 55);
          barWidth = 120;
        }

        int barHeight = 6;
        int barX = 120 - barWidth / 2;
        int barY = 85; 
        float phaseRatio = (phaseDuration > 0) ? (float)(phaseDuration - phaseTicksRemaining) / phaseDuration : 1.0;
        int progressWidth = barWidth * phaseRatio;
        
        canvas.fillRect(barX, barY, barWidth, barHeight, COLOR_BAR_BG);
        if (progressWidth > 0) {
          canvas.fillRect(barX + barWidth - progressWidth, barY, progressWidth, barHeight, COLOR_BAR_FG);
        }

        canvas.setTextDatum(bottom_left);
        canvas.setTextSize(2);
        int m = totalSecondsRemaining / 60;
        int s = totalSecondsRemaining % 60;
        char timeStr[10];
        snprintf(timeStr, sizeof(timeStr), "%d:%02d", m, s);
        canvas.drawString(timeStr, 5, 120);

        int totalBarWidth = 240;
        int totalBarHeight = 12;
        float totalRatio = (totalDuration > 0) ? (float)(totalDuration - totalSecondsRemaining) / totalDuration : 1.0;
        int totalProgressWidth = totalBarWidth * totalRatio;
        
        canvas.fillRect(0, 123, totalBarWidth, totalBarHeight, COLOR_BAR_BG);
        if (totalProgressWidth > 0) {
          canvas.fillRect(totalBarWidth - totalProgressWidth, 123, totalProgressWidth, totalBarHeight, COLOR_BAR_FG);
        }
      }
    } else {
      // LED Theme Layout
      // Top Left: Pattern Name
      canvas.setTextDatum(top_left);
      canvas.setTextSize(1);
      canvas.drawString(activePattern->name, 10, 5);
      
      // Top Right: Total Time String
      canvas.setTextDatum(top_right);
      canvas.drawString(activePattern->totalTimeStr, 230, 5);
      
      // Center: Phase and Counter
      String phaseText = "";
      switch(currentPhase) {
        case PHASE_STARTING: phaseText = "Starting in"; break;
        case PHASE_BREATH_IN: phaseText = "breath in"; break;
        case PHASE_HOLD_FULL: phaseText = activePattern->hasHoldEmpty ? "hold full" : "hold"; break;
        case PHASE_BREATH_OUT: phaseText = "breath out"; break;
        case PHASE_HOLD_EMPTY: phaseText = "hold empty"; break;
      }
      
      canvas.setTextDatum(middle_center);
      canvas.setTextSize(2);
      canvas.drawString(phaseText, 120, 45);
      canvas.setTextSize(4);
      canvas.drawString(String(phaseTicksRemaining), 120, 80);
      
      // Bottom Left: Help
      canvas.setTextDatum(bottom_left);
      canvas.setTextSize(1);
      canvas.drawString("h: help", 5, 130);
      
      // Bottom Right: Total time left
      canvas.setTextDatum(bottom_right);
      canvas.setTextSize(2);
      int m = totalSecondsRemaining / 60;
      int s = totalSecondsRemaining % 60;
      char trStr[10];
      snprintf(trStr, sizeof(trStr), "%d:%02d", m, s);
      canvas.drawString(trStr, 235, 130);
    }
  }

  if (showHelp) {
    drawHelpOverlay();
  }

  // Atomic buffer swap with SPI protection (v0.7 optimization)
  M5Cardputer.Display.startWrite();
  canvas.pushSprite(0, 0);
  M5Cardputer.Display.endWrite();
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(volumeLevel);
  M5Cardputer.Display.setBrightness(brightnessLevel);
  
  randomSeed(analogRead(0));
  initColorPool();
  pickRandomLEDColor(); // Pick initial color for first boot
  
  canvas.createSprite(240, 135);
  M5Cardputer.Display.setRotation(1); // Ensure landscape
  lastInteractionTime = millis();
  drawScreen();
}

void loop() {
  M5Cardputer.update();
  bool needRedraw = false;

  if (M5Cardputer.Keyboard.isChange()) {
    if (M5Cardputer.Keyboard.isPressed()) {
      lastInteractionTime = millis();
      if (autoDisplayOff) {
        autoDisplayOff = false;
        M5Cardputer.Display.setBrightness(brightnessLevel);
        return; // v1.0: Consume the first key press to only wake the screen
      }
      Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

      bool escPressed = false;
      char keyChar = 0;

      for (auto i : status.word) {
        if (i == '`' || i == 27) {
          escPressed = true;
        } else {
          keyChar = i;
        }
      }

      if (escPressed) {
        if (currentState != MAIN_MENU) {
          currentState = MAIN_MENU;
          needRedraw = true;
        }
      }

      if (keyChar > 0) {
        // Global key actions
        if (keyChar == 's' || keyChar == 'S') {
          soundEnabled = !soundEnabled;
          needRedraw = true;
        } else if (keyChar == 'd' || keyChar == 'D') {
          screenOn = !screenOn;
          M5Cardputer.Display.setBrightness(screenOn ? brightnessLevel : 0);
        } else if (keyChar == '-') {
          volumeLevel = max(0, volumeLevel - 16);
          M5Cardputer.Speaker.setVolume(volumeLevel);
        } else if (keyChar == '+' || keyChar == '=') {
          volumeLevel = min(255, volumeLevel + 16);
          M5Cardputer.Speaker.setVolume(volumeLevel);
        } else if (keyChar == '.') {
          brightnessLevel = max(0, brightnessLevel - 16);
          if (screenOn) M5Cardputer.Display.setBrightness(brightnessLevel);
        } else if (keyChar == ';' || keyChar == ':') {
          brightnessLevel = min(255, brightnessLevel + 16);
          if (screenOn) M5Cardputer.Display.setBrightness(brightnessLevel);
        } else if (keyChar == 'a' || keyChar == 'A') {
          if (currentTheme == THEME_MONOCHROME) {
            currentTheme = THEME_LED;
            pickRandomLEDColor(); // New random color on each switch to LED
          } else {
            currentTheme = THEME_MONOCHROME;
          }
          needRedraw = true;
        } else if (keyChar == 'h' || keyChar == 'H') {
          showHelp = !showHelp;
          needRedraw = true;
        }

        // State-specific key actions
        if (currentState == MAIN_MENU) {
          if (keyChar == '1') {
            activePattern = &patternSquare;
            startPattern();
            needRedraw = true;
          } else if (keyChar == '2') {
            activePattern = &patternTriangle;
            startPattern();
            needRedraw = true;
          } else if (keyChar == '3') {
            activePattern = &patternCombo;
            setComboPattern();
            startPattern();
            needRedraw = true;
          }
        } else if (currentState == RUNNING_PATTERN) {
          if (keyChar == ' ') {
            isPaused = !isPaused;
            needRedraw = true;
          }
        }
      }
    }
  }

  // Timer tick logic
  if (currentState == RUNNING_PATTERN && !isPaused) {
    unsigned long now = millis();
    if (now - lastTickTime >= 1000) {
      lastTickTime = now;
      phaseTicksRemaining--;

      if (currentPhase != PHASE_STARTING) {
        totalSecondsRemaining--;
        if (totalSecondsRemaining <= 0) {
          // Pattern finished
          currentState = MAIN_MENU;
          needRedraw = true;
          // v1.0: Triple-bleep completion (same as combo transition style)
          for(int i=0; i<3; i++) {
            playBeep(2000, 30);
            if (i < 2) delay(40);
          }
        }
      }

      if (currentState == RUNNING_PATTERN && phaseTicksRemaining <= 0 && totalSecondsRemaining > 0) {
        advancePhase();
        needRedraw = false; // already redrawn inside advancePhase if needed
      }

      needRedraw = true;
    }
  }

  if (needRedraw) {
    drawScreen();
  }

  // v0.9: Inactivity Power Management
  if (currentState == RUNNING_PATTERN) {
    lastInteractionTime = millis(); // Session prevents inactivity timeout
  } else {
    unsigned long inactiveMs = millis() - lastInteractionTime;
    if (inactiveMs > 30000 && !autoDisplayOff && screenOn) {
      autoDisplayOff = true;
      M5Cardputer.Display.setBrightness(0);
    }
    if (inactiveMs > 300000) {
      M5Cardputer.Power.powerOff();
    }
  }

  delay(10);
}
