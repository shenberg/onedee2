#include "FastLED.h"

FASTLED_USING_NAMESPACE

#define DATA_PIN    6
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    300

CRGBArray<NUM_LEDS> leds;

#define BRIGHTNESS          96
#define FRAMES_PER_SECOND  120

template<class T, size_t N>
constexpr int length(const T (&arr)[N]) {
  return N;
}

constexpr int LOCAL_BEAT_DELAY = 10; // milliseconds

// from onedee_slave
constexpr int LED_PIN = 3;      // led connected to digital pin 6 (pwm)
constexpr int KNOCK_SENSOR_PIN = A0; // the piezo is connected to analog pin 0
constexpr int POT_PIN = A3; //pin A0 to read analog input
constexpr int COMMUNICATION_PIN = 9; // read remote drum beat
constexpr int REMOTE_BEAT_DELAY = 15; // milliseconds


bool isKnock() {
  // ronen suggested analog noise workaround
  /*
  analogRead(POT_PIN);
  delay(1);
  */
  int potValue = analogRead(POT_PIN);          //Read and save analog potValue from potentiometer
  int threshold = potValue; // initial threshold value to decide when the detected sound is a knock or not

  int ledPotValue = map(potValue, 0, 1023, 0, 255); //Map potValue 0-1023 to 0-255 (PWM) = ledPotValue
  analogWrite(LED_PIN, ledPotValue);          //Send PWM ledPotValue to led

  Serial.print(potValue);
  Serial.print(' ');
  // read the sensor and store it in the variable sensorReading:
  // ronen suggested analog noise workaround
  /*
  analogRead(KNOCK_SENSOR_PIN);
  delay(1);
  */
  int sensorReading = analogRead(KNOCK_SENSOR_PIN);  
  Serial.println(sensorReading);

  if (sensorReading >= threshold) {
    Serial.print("Knock!");
    Serial.println(threshold);
    // TODO: some form of cooldown to debounce
    return true;    
  }
  return false;
}

/*
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
*/

bool remoteDrumBeat(long now) {
  static long lastRemoteBeatTime = 0;

  // only allow one beat per REMOTE_BEAT_DELAY milliseconds from remote side
  if (now - lastRemoteBeatTime < REMOTE_BEAT_DELAY) {
    return false;
  }

  // if pin is set to high then remote side has recently detected a beat
  if (digitalRead(COMMUNICATION_PIN) == HIGH) {
    // yay, beat detected, update last detected beat time
    lastRemoteBeatTime = now;
    return true;
  }

  return false;
}

void addRemoteBeat(long now) {
  // TODO different effects
  Serial.println("REMOTE beat detected");
}

bool localDrumBeat(long now) {
  static long lastLocalBeatTime = 0;

  // only allow one beat per LOCAL_BEAT_DELAY milliseconds from remote side
  if (now - lastLocalBeatTime < LOCAL_BEAT_DELAY) {
    return false;
  }

  if (isKnock()) {
    lastLocalBeatTime = now;
    return true;
  }

  return false;
}

void addLocalBeat(long now) {
  // TODO different effects
  Serial.println("LOCAL beat detected");  
}

void updateWorld(long now, int dt) {
  //TODO
}

void drawWorld() {
  //TODO
}

long lastTime = 0;
void loop() {

  long now = millis();
  int dt = now - lastTime;

  if (remoteDrumBeat(now)) {
    addRemoteBeat(now);
  }

  if (localDrumBeat(now)) {
    addLocalBeat(now);
  }

  updateWorld(now, dt);
  drawWorld();
  
  FastLED.show();
  delay(1000 / FRAMES_PER_SECOND);
}

void setup() {
  // from onedee_slave
  pinMode(POT_PIN, INPUT); //Optional 
  pinMode(LED_PIN, OUTPUT); // declare the LED_PIN as as OUTPUT
  pinMode(COMMUNICATION_PIN, OUTPUT);


  // initialize the pushbutton pin as an input:
  Serial.begin(115200);
  Serial.println("begin");

    // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);

  // last update time
  lastTime = millis();
}
