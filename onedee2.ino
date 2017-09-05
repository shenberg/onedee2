#include "FastLED.h"

FASTLED_USING_NAMESPACE

#define DATA_PIN    5
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    300

CRGBArray<NUM_LEDS> leds;

#define BRIGHTNESS          28
#define FRAMES_PER_SECOND  30

template<class T, size_t N>
constexpr int length(const T (&arr)[N]) {
  return N;
}

constexpr int TIME_BETWEEN_FRAMES = 1000 / FRAMES_PER_SECOND; // in milliseconds

constexpr int LOCAL_BEAT_DELAY = 15; // milliseconds
constexpr int NUM_SIMULTANEOUS_BEATS = 10;
constexpr int BEAT_TRAVEL_TIME = 3000; // in milliseconds

const CRGB LOCAL_BEAT_COLOR = CRGB(255, 0, 0);
const CRGB REMOTE_BEAT_COLOR = CRGB(0, 255, 0);

// TODO: hack
long remoteBeatTimes[NUM_SIMULTANEOUS_BEATS] = {-10000};
long localBeatTimes[NUM_SIMULTANEOUS_BEATS] = {-10000};
int localCurrentBeat = 0;
int remoteCurrentBeat = 0;


// from onedee_slave
constexpr int LED_PIN = 3;      // led connected to digital pin 6 (pwm)
constexpr int KNOCK_SENSOR_PIN = A0; // the piezo is connected to analog pin 0
constexpr int POT_PIN = A3; //pin A0 to read analog input
constexpr int COMMUNICATION_PIN = 9; // read remote drum beat
constexpr int REMOTE_BEAT_DELAY = 15; // milliseconds


bool isKnock() {
  // simple hysteresis - don't tap again until we see a release sample
  static bool wasKnocked = false;
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
    if (!wasKnocked) {
      Serial.print("Knock!");
      Serial.println(threshold);
      // TODO: some form of cooldown to debounce
      wasKnocked = true;
      return true;
    }

  } else if (wasKnocked) {
    Serial.println("Unknocked");
    wasKnocked = false;
  }
  return false;
}

bool remoteDrumBeat(long now) {
  static long lastRemoteBeatTime = 0;
  static bool isBeating = false;

  // only allow one beat per REMOTE_BEAT_DELAY milliseconds from remote side
  if (now - lastRemoteBeatTime < REMOTE_BEAT_DELAY) {
    return false;
  }

  // if pin is set to high then remote side has recently detected a beat
  if (digitalRead(COMMUNICATION_PIN) == HIGH) {
    if (!isBeating) {
      // yay, beat detected, update last detected beat time
      lastRemoteBeatTime = now;
      return true;
    }
  } else {
    if (isBeating) {
      Serial.println("unbeat");
      isBeating = false;
    }
  }

  return false;
}

void addRemoteBeat(long now) {
  // TODO different effects
  Serial.println("REMOTE beat detected");
  remoteBeatTimes[remoteCurrentBeat] = now;
  ++remoteCurrentBeat;
  if (remoteCurrentBeat >= NUM_SIMULTANEOUS_BEATS) {
    remoteCurrentBeat = 0;
  }
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
  localBeatTimes[localCurrentBeat] = now;
  ++localCurrentBeat;
  if (localCurrentBeat >= NUM_SIMULTANEOUS_BEATS) {
    localCurrentBeat = 0;
  }
}

void updateWorld(long now, int dt) {
  //TODO
}

void drawLocalBeat(long beatTime, long now, CRGB color) {
  long timeFromStart = now - beatTime;
  if (timeFromStart >= BEAT_TRAVEL_TIME) {
    return;
  }

  int position = timeFromStart * NUM_LEDS / BEAT_TRAVEL_TIME;
  if (position >= NUM_LEDS) {
    Serial.println("EEEEEEK");
    return;
  }

  leds[position] += color;
}

void drawRemoteBeat(long beatTime, long now, CRGB color) {
  long timeFromStart = now - beatTime;
  if (timeFromStart >= BEAT_TRAVEL_TIME) {
    return;
  }

  int position = timeFromStart * NUM_LEDS / BEAT_TRAVEL_TIME;
  if (position >= NUM_LEDS) {
    Serial.println("AAAAAAK2");
    return;
  }

  leds[(NUM_LEDS - 1) - position] += color;
}

void drawWorld(long now) {
  leds.fadeToBlackBy(30);
  for(int i = 0; i < NUM_SIMULTANEOUS_BEATS; i++) {
    drawLocalBeat(localBeatTimes[i], now, LOCAL_BEAT_COLOR);
    drawRemoteBeat(remoteBeatTimes[i], now, REMOTE_BEAT_COLOR);
  }
}

long lastTime = 0;
long lastDrawTime = 0;

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

  if (now - lastDrawTime > TIME_BETWEEN_FRAMES) {
    drawWorld(now);
    lastDrawTime = now;
    
    FastLED.show();
  }
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

  // 5 second startup delay in case we screw up delays in the code
  delay(5000);
}
