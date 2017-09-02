#include "FastLED.h"

FASTLED_USING_NAMESPACE

#define DATA_PIN    6
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    64

CRGBArray<NUM_LEDS> leds;

#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120

template<class T, size_t N>
constexpr int length(const T (&arr)[N]) {
  return N;
}

constexpr int DOWNGRADE_THRESHOLD = 2;

constexpr int BUTTON_PIN = 2;     // the number of the pushbutton pin

constexpr int DEBOUNCE_TIME = 20; // milliseconds - time given for button-press to complete

long debounceEnd = 0;
enum {
    WAITING_FOR_PRESS,
    DEBOUNCE,
    WAITING_FOR_LEAVE
} logicState;

bool was_button_pressed(int currentState) {

  switch(logicState) {
    case WAITING_FOR_PRESS:
      if (currentState) {
        logicState = DEBOUNCE;
        debounceEnd = millis() + DEBOUNCE_TIME;
      }
      break;
    case DEBOUNCE:
      if (millis() >= debounceEnd) {
        if (currentState) {
          logicState = WAITING_FOR_LEAVE;
          return true;
        } else {
          logicState = WAITING_FOR_PRESS;
        }
      }
      break;
    case WAITING_FOR_LEAVE:
      if (!currentState) {
        logicState = WAITING_FOR_PRESS;
      }
      break;
  }
  return false;
}

//const CRGB EMPTY_SPOT = CRGB(0,64,255);
const CRGB EMPTY_SPOT = CRGB(0,0,0);
const CRGB FILLED_SPOT = CRGB(255,64,0);


long startTime = 0; // bpm relative to here
int level = 0;
int position = 0;
int lossCount = 0;

void reset_game() {
  startTime = millis();
  logicState = WAITING_FOR_PRESS;
  position = 0;
}

constexpr bool tempo[] = {false, false, false, true, false, true};
//constexpr bool tempo2[4] = {false, false, true, true};
//constexpr bool tempos[][] = {tempo1, tempo2, tempo1, tempo1, tempo2};
constexpr int bpm = 120;

void fill_leds_with_tempo() {
  long time = (millis() - startTime);
  int beat = ((bpm * time) / 60000) % length(tempo);
  leds[NUM_LEDS - 1] = leds[0] = EMPTY_SPOT;
  for (int i = 1; i < NUM_LEDS - 1; i++) {
    if (tempo[beat]) {
      leds[i] = FILLED_SPOT;
    } else {
      leds[i] = EMPTY_SPOT;
    }
    beat = (beat + 1) % length(tempo);
  }
}

void button_pressed() {
  int nextPos;
  for (nextPos = (position + 1) % NUM_LEDS; 
       leds[nextPos] != EMPTY_SPOT;
       nextPos = (nextPos + 1) % NUM_LEDS) {
  }
  position = nextPos;
}

void next_level() {
  level++; // TODO: actual logic
}

void lower_level() {
  if (level > 0) {
    level--;
  }
}

boolean collision(int position) {
  return leds[position] != EMPTY_SPOT;
}

boolean won(int position) {
  return position == NUM_LEDS - 1;
}

void draw_player(int position) {
  leds[position] = CRGB::Green;
}

void lose_animation() {
  for (int i = 0; i < 3; i++) {
    leds[position] = CRGB::Yellow;
    FastLED.show();
    delay(100);
    leds[position] = CRGB::Black;
    FastLED.show();
    delay(100);    
  }
}

void win_animation() {
  for(int i = NUM_LEDS - 1; i > 0; i--) {
    leds(i, NUM_LEDS - 1).fadeToBlackBy(73);
    leds[i] = CRGB::Green;
    FastLED.show();
    delay(10);
  }
  for(int i = 0; i < 100; i++) {
    leds.fadeToBlackBy(50);
    FastLED.show();
    delay(10);
  }
}

void lose() {
  lose_animation();
  lossCount += 1;
  if (lossCount >= DOWNGRADE_THRESHOLD) {
    lower_level();
    lossCount = 0;
  }
  reset_game();  
}

void win() {
  win_animation();
  next_level();
  lossCount = 0;
  reset_game();  
}

void loop() {
  // read the state of the pushbutton value:
  int buttonState = digitalRead(BUTTON_PIN);
  fill_leds_with_tempo();
  if (collision(position)) {
    lose();
    return;
  }
  // NOTE: order matters since we compare to the leds array for collision detection :(
  draw_player(position);

  if (won(position)) {
    win();
    return;
  }
  // check if the pushbutton is pressed.
  // if it is, the buttonState is HIGH:
  if (was_button_pressed(buttonState)) {
    button_pressed();
  }
  
  FastLED.show();
  delay(1000 / FRAMES_PER_SECOND);
}

void setup() {
  // initialize the pushbutton pin as an input:
  pinMode(BUTTON_PIN, INPUT);
  Serial.begin(9600);
  Serial.println("begin");

    // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  win_animation();
  reset_game();
}
