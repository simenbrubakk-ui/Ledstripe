#include <Wire.h> 
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <FastLED.h>
#define NUM_LEDS   32
#define DATA_PIN   9
// --- BME280 ---
#define BME280_ADDR 0x76
Adafruit_BME280 bme;
bool TMP = false;
// --- OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_ADDR    0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// --- Knapper ---
const uint8_t buttonPins[] = {4, 5, 6};        
const uint8_t numButtons = sizeof(buttonPins);
// --- LED ---
CRGB leds[NUM_LEDS];
uint8_t brightnessLevel = 1;       
             
// --- Tilstand ---
bool ledsOn  = false;
bool ledsOne = true;   // bank 1
bool ledsTwo = false;  // bank 2
uint8_t mode = 0;                                 
bool Disp_refresh = true;                           //  oppdater OLED én gang per loop
// --- Debounce / long press ---
uint8_t lastButtonState[3] = {HIGH, HIGH, HIGH};  
uint8_t buttonState[3]     = {HIGH, HIGH, HIGH};  
unsigned long lastDebounceTime[3] = {0, 0, 0};
const unsigned long debounceDelay = 50;
const unsigned long longPressTime = 500;           // Holde inn knapp for å endre bank
unsigned long pressedTime[3] = {0, 0, 0};
bool longPressHandled[3] = {false, false, false};
// --- Sensor/OLED tid ---
unsigned long lastRead = 0;
const unsigned long interval_lesing = 1000;
int16_t sistetemp = -32768; 
// ----------------- fremovererklæringer av moduser -----------------
void mode0();  void mode1();  void mode2();  void mode3();
void mode4();  void mode5();  void mode6();  void mode7();
void mode8();  void mode9();  void mode10(); void mode11();
void modeThermometer();
// ----------------- OLED tegning -----------------
void drawOLED() {
  display.clearDisplay();
  display.setTextWrap(false);                              
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  // Topp: status
  display.setCursor(100, 0);
  display.print(ledsOn ? F(" On") : F(" Off"));
  display.setCursor(0, 24);
  display.print(F("Bank: "));
  display.print(ledsOne ? 1 : 2);
  display.setCursor(0, 16);
  display.print(F("Preset: "));
  display.print(mode);
  // Brightness (kun heltall)
  uint8_t b = FastLED.getBrightness();                     // 0..255
  int pct  = (b * 100 + 127) / 255;                        // avrundet %
  int pct10 = ((pct + 5) / 10) * 10;                       // 0,10,...,100
  if (pct10 < 0) pct10 = 0; if (pct10 > 100) pct10 = 100;
  display.setCursor(0, 36);
  display.print(F("Brightness: "));
  display.print(brightnessLevel);
  display.print(F(" ("));
  display.print(pct10);
  display.print(F("%)"));
  // Temperatur stort, uten float-print
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(F(" "));
  if (!TMP || sistetemp == -32768) {
    display.print(F("--.-"));
  } else {
    int t10 = sistetemp;
    bool neg = t10 < 0; if (neg) t10 = -t10;
    int whole = t10 / 10;
    int frac  = t10 % 10;
    if (neg) display.print(F("-"));
    display.print(whole);
    display.print(F("."));
    display.print(frac);
  }
  display.print(F("C"));
  //  sikker “visking” av nederste linje mot artefakter
  display.fillRect(0, 56, 128, 8, SSD1306_BLACK);
  display.display();
}
// ----------------- SETUP -----------------
void setup() {
  Serial.begin(9600);
  Wire.begin();
  TMP = bme.begin(BME280_ADDR);            
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(25 * brightnessLevel);
  for (uint8_t i = 0; i < numButtons; i++) pinMode(buttonPins[i], INPUT_PULLUP);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (1) {} 
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Start"));
  display.display();
  delay(300);
  drawOLED();
}
// ----------------- LOOP -----------------
void loop() {
  // Knapper
  for (uint8_t i = 0; i < numButtons; i++) {
    uint8_t reading = digitalRead(buttonPins[i]);
    if (reading != lastButtonState[i]) lastDebounceTime[i] = millis();
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != buttonState[i]) {
        buttonState[i] = reading;
        if (buttonState[i] == LOW) {
          pressedTime[i] = millis();
          longPressHandled[i] = false;
        } else {
          unsigned long pressDuration = millis() - pressedTime[i];
          if (pressDuration < longPressTime) {
            if (i == 0) {                         // av/på
              ledsOn = !ledsOn;
              if (!ledsOn) fill_solid(leds, NUM_LEDS, CRGB::Black);
              Disp_refresh = true;                  
            } else if (i == 1) {                  // neste modus
              mode = (mode + 1) % (ledsOne ? 5 : 8);
              Disp_refresh = true;                  
            } else if (i == 2) {                  // lysstyrke
              brightnessLevel++;
              if (brightnessLevel > 10) brightnessLevel = 1;
              uint16_t newBrightness = 25 * brightnessLevel;
              if (newBrightness > 255) newBrightness = 255;
              FastLED.setBrightness((uint8_t)newBrightness);
              Disp_refresh = true;                  
            }
          }
        }
      }
    }
    // langt trykk: bytt bank
    if (buttonState[i] == LOW && !longPressHandled[i]) {
      unsigned long holdTime = millis() - pressedTime[i];
      if (holdTime > longPressTime) {
        longPressHandled[i] = true;
        if (i == 1) {
          ledsOne = !ledsOne;
          ledsTwo = !ledsTwo;
          mode = 0;
          Disp_refresh = true;                      
        }
      }
    }
    lastButtonState[i] = reading;
  }
  // LED-modi
  if (ledsOn) {
    if (ledsOne) {
      if      (mode == 0) mode0();
      else if (mode == 1) mode1();
      else if (mode == 2) mode2();
      else if (mode == 3) mode3();
      else if (mode == 4) {
        if (TMP) modeThermometer();
        else        fill_solid(leds, NUM_LEDS, CRGB::Black);
      }
    } else {
      if      (mode == 0) mode4();
      else if (mode == 1) mode5();
      else if (mode == 2) mode6();
      else if (mode == 3) mode7();
      else if (mode == 4) mode8();
      else if (mode == 5) mode9();
      else if (mode == 6) mode10();
      else if (mode == 7) mode11();
    }
  } else {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
  }
  FastLED.show();
  // Les temperatur sjeldnere og tegn OLED etterpå
  unsigned long now = millis();
  if (now - lastRead >= interval_lesing) {
    lastRead = now;
    if (TMP) {
      
      // les som float, konverter til *10 heltall
      float t = bme.readTemperature();
      if (isnan(t)) sistetemp = -32768;
      else          sistetemp = (int16_t)(t * 10.0f);
    } else {
      sistetemp = -32768;
    }
    Disp_refresh = true;                            
  }
  if (Disp_refresh) {                               //  tegn OLED kun her
    drawOLED();
    Disp_refresh = false;
  }
}
// ---------------- MODUSER ----------------
void mode0(){ // rask hvit blink
  static unsigned long previousMillis = 0;
  const unsigned long interval = 50;
  static bool ledState = false;
  unsigned long now = millis();
  if (now - previousMillis >= interval){
    previousMillis = now;
    ledState = !ledState;
  }
  fill_solid(leds, NUM_LEDS, ledState ? CRGB::White : CRGB::Black);
}
void mode1(){ // fast hue
  static unsigned long previousMillis = 0;
  const unsigned long interval = 35;
  static uint8_t hue = 0;
  unsigned long now = millis();
  if (now - previousMillis >= interval){
    previousMillis = now;
    hue++;
  }
  fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
}
void mode2(){ // regnbue langs stripen
  static uint8_t hue = 0;
  for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(hue + (i * 20), 255, 255);
  hue++;
}
void mode3(){ // løpende enkelt LED i regnbue
  const uint8_t tailLength = 7;
  static uint8_t currentLED = 0;
  static unsigned long previousMillis = 0;
  const unsigned long interval = 200;
  unsigned long now = millis();
  if (now - previousMillis >= interval) {
    previousMillis = now;
    static uint8_t hue = 0;
    hue += 20;
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (uint8_t j = 0; j < tailLength; j++) {
      uint8_t index = (currentLED + j) % NUM_LEDS;
      leds[index] = CHSV(hue, 255, 255);
    }
    currentLED = (currentLED + 1) % NUM_LEDS;
  }
}
// Termometer (bank 1, modus 4)
void modeThermometer() {
  if (!TMP) return;
  static unsigned long lastUpdate = 0;
  const unsigned long interval = 1000;
  unsigned long now = millis();
  if (now - lastUpdate < interval) return;
  lastUpdate = now;
  float temp = bme.readTemperature();
  if (isnan(temp)) return;
  sistetemp = (int16_t)(temp * 10.0f);
  const float T_MIN = 0.0f;
  const float T_MAX = 32.0f;
  long t10    = (long)(temp * 10.0f);
  long tmin10 = (long)(T_MIN * 10.0f);
  long tmax10 = (long)(T_MAX * 10.0f);
  if (t10 < tmin10) t10 = tmin10;
  if (t10 > tmax10) t10 = tmax10;
  int totalLit = map(t10, tmin10, tmax10, 0, NUM_LEDS);
  int n1 = NUM_LEDS / 3;
  int n3 = NUM_LEDS / 3;
  int n2 = NUM_LEDS - n1 - n3;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = 0; i < totalLit; ++i) {
    if      (i < n1)          leds[i] = CRGB::Blue;
    else if (i < n1 + n2)     leds[i] = CRGB::White;
    else                      leds[i] = CRGB::Red;
  }
}
// Bank 2: helfarger
void mode4(){  fill_solid(leds, NUM_LEDS, CRGB(255,0,0));    }
void mode5(){  fill_solid(leds, NUM_LEDS, CRGB(255,140,0));      }
void mode6(){  fill_solid(leds, NUM_LEDS, CRGB(255,211,0));     }
void mode7(){  fill_solid(leds, NUM_LEDS, CRGB(0,255,0));    }
void mode8(){  fill_solid(leds, NUM_LEDS, CRGB(0,255,128));   }
void mode9(){  fill_solid(leds, NUM_LEDS, CRGB(0,0,255));}
void mode10(){ fill_solid(leds, NUM_LEDS, CRGB(128,0,128));     }
void mode11(){ fill_solid(leds, NUM_LEDS, CRGB(255,0,64));   }