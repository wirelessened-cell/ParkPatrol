#include <WiFi.h>
#include <HTTPClient.h>

// WIFI + THINGSPEAK

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

const char* THINGSPEAK_API_KEY = "UIX8VI6ZD2IMLN4P";

const char* THINGSPEAK_URL =
  "http://api.thingspeak.com/update";


// PIN DEFINITIONS
// HC-SR04 ultrasonic sensor
#define TRIG_PIN 5
#define ECHO_PIN 18

// PIR motion sensor
#define PIR_PIN 19

// LEDs
#define GREEN_LED 25
#define YELLOW_LED 26
#define RED_LED 27

// Buzzer
#define BUZZER_PIN 23


// SETTINGS

// Car ARRIVES when distance is less than 30 cm
const float OCCUPIED_THRESHOLD_CM = 30.0;

// Car LEAVES when distance is greater than 40 cm
const float VACANT_THRESHOLD_CM = 40.0;

// Demo parking limit = 10 seconds
const unsigned long PARKING_LIMIT_MS = 10000;

// Upload to ThingSpeak every 20 seconds
const unsigned long THINGSPEAK_INTERVAL = 20000;


// PARKING STATES

enum ParkingState {

  VACANT,                // 0
  OCCUPIED,              // 1
  OCCUPIED_OVERTIME,     // 2
  DEPARTING_UPLOAD,      // 3
  FAILSAFE               // 4

};

ParkingState currentState = VACANT;


// GLOBAL VARIABLES

unsigned long occupiedStartTime = 0;

unsigned long lastThingSpeakUpload = 0;

float distanceCM = 0;

bool pirDetected = false;


// SETUP

void setup() {

  Serial.begin(115200);

  // Sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(PIR_PIN, INPUT);

  // LED pins
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);


  Serial.println();
  Serial.println("==============================");
  Serial.println("SMART PARKING SYSTEM STARTED");
  Serial.println("==============================");


  // CONNECT TO WOKWI WIFI
  Serial.print("Connecting to WiFi");

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD,
    6
  );

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  Serial.println("WiFi connected!");

  Serial.print("IP Address: ");

  Serial.println(WiFi.localIP());

  Serial.println();
}


// MAIN LOOP

void loop() {

  // Read sensors
  distanceCM = readDistance();

  pirDetected = digitalRead(PIR_PIN);


  Serial.print("Distance: ");

  Serial.print(distanceCM);

  Serial.print(" cm | PIR: ");

  Serial.print(pirDetected);

  Serial.print(" | State: ");

  Serial.println(getStateName());


  updateParkingState();


  // Upload data to ThingSpeak every 20 seconds
  if (
    millis() - lastThingSpeakUpload
    >= THINGSPEAK_INTERVAL
  ) {

    uploadToThingSpeak();

    lastThingSpeakUpload = millis();
  }

  delay(500);
}


// READ HC-SR04 DISTANCE

float readDistance() {

  // Ensure trigger starts LOW
  digitalWrite(TRIG_PIN, LOW);

  delayMicroseconds(2);


  // Send 10 microsecond ultrasonic pulse
  digitalWrite(TRIG_PIN, HIGH);

  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);


  // Measure echo pulse
  long duration =
    pulseIn(
      ECHO_PIN,
      HIGH,
      30000
    );


  // No echo received
  if (duration == 0) {

    return -1;
  }


  // Convert echo time into distance in centimetres
  float distance =
    duration * 0.0343 / 2;


  return distance;
}


// FINITE STATE MACHINE

void updateParkingState() {

  switch (currentState) {


    // STATE 0: VACANT

    case VACANT:

      // Green LED
      setLights(
        HIGH,
        LOW,
        LOW
      );

      // Buzzer OFF
      digitalWrite(BUZZER_PIN, LOW);


      // Sensor error
      if (distanceCM < 0) {

        Serial.println(
          "Invalid ultrasonic reading"
        );

        currentState = FAILSAFE;
      }


      // Vehicle detected
      else if (
        distanceCM < OCCUPIED_THRESHOLD_CM &&
        pirDetected == true
      ) {

        Serial.println(
          ">>> VEHICLE ARRIVAL CONFIRMED"
        );

        occupiedStartTime = millis();

        currentState = OCCUPIED;
      }

      break;


    // STATE 1: OCCUPIED

    case OCCUPIED:

      // Yellow LED
      setLights(
        LOW,
        HIGH,
        LOW
      );

      // Buzzer OFF
      digitalWrite(BUZZER_PIN, LOW);


      // Check ultrasonic sensor
      if (distanceCM < 0) {

        currentState = FAILSAFE;
      }


      // Vehicle has left
      else if (
        distanceCM > VACANT_THRESHOLD_CM
      ) {

        currentState =
          DEPARTING_UPLOAD;
      }


      // Vehicle has exceeded parking limit
      else if (
        millis() - occupiedStartTime
        >= PARKING_LIMIT_MS
      ) {

        Serial.println(
          ">>> PARKING TIME EXCEEDED"
        );

        currentState =
          OCCUPIED_OVERTIME;
      }

      break;


    // STATE 2: OCCUPIED OVERTIME

    case OCCUPIED_OVERTIME:

      // Red LED
      setLights(
        LOW,
        LOW,
        HIGH
      );

      // Buzzer ON
      digitalWrite(BUZZER_PIN, HIGH);


      // Sensor error
      if (distanceCM < 0) {

        currentState = FAILSAFE;
      }


      // Vehicle leaves
      else if (
        distanceCM > VACANT_THRESHOLD_CM
      ) {

        currentState =
          DEPARTING_UPLOAD;
      }

      break;


    // STATE 3: DEPARTING / UPLOAD

    case DEPARTING_UPLOAD:

      // Yellow while processing
      setLights(
        LOW,
        HIGH,
        LOW
      );

      // Buzzer OFF
      digitalWrite(BUZZER_PIN, LOW);


      Serial.println(
        ">>> VEHICLE DEPARTED"
      );

      Serial.println(
        ">>> PARKING SESSION COMPLETE"
      );


      // Return bay to vacant
      currentState = VACANT;

      break;


    // STATE 4: FAILSAFE

    case FAILSAFE:

      Serial.println(
        ">>> WARNING: SENSOR FAILURE"
      );

      // Buzzer OFF
      digitalWrite(BUZZER_PIN, LOW);


      // Green and yellow OFF
      digitalWrite(
        GREEN_LED,
        LOW
      );

      digitalWrite(
        YELLOW_LED,
        LOW
      );


      // Flash red LED
      digitalWrite(
        RED_LED,
        HIGH
      );

      delay(250);

      digitalWrite(
        RED_LED,
        LOW
      );

      delay(250);


      // Try sensor again
      distanceCM =
        readDistance();


      if (distanceCM > 0) {

        Serial.println(
          ">>> SENSOR RECOVERED"
        );

        currentState = VACANT;
      }

      break;
  }
}


// LED CONTROL

void setLights(
  bool green,
  bool yellow,
  bool red
) {

  digitalWrite(
    GREEN_LED,
    green
  );

  digitalWrite(
    YELLOW_LED,
    yellow
  );

  digitalWrite(
    RED_LED,
    red
  );
}


// THINGSPEAK UPLOAD

void uploadToThingSpeak() {

  // Check Wi-Fi
  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println(
      "ThingSpeak upload skipped - WiFi disconnected"
    );

    return;
  }


  HTTPClient http;


  // Occupancy value
  // 0 = vacant
  // 1 = occupied

  int occupancy = 0;


  if (
    currentState == OCCUPIED ||
    currentState == OCCUPIED_OVERTIME
  ) {

    occupancy = 1;
  }


  // Parking duration
  unsigned long parkingDuration = 0;


  if (occupancy == 1) {

    parkingDuration =
      (millis() - occupiedStartTime)
      / 1000;
  }


  // Build ThingSpeak URL
  String url =
    String(THINGSPEAK_URL);


  // API key
  url += "?api_key=";

  url +=
    THINGSPEAK_API_KEY;


  // FIELD 1
  // Distance in cm

  url += "&field1=";

  url +=
    String(distanceCM);


  // FIELD 2
  // Occupancy
  // 0 = vacant
  // 1 = occupied

  url += "&field2=";

  url +=
    String(occupancy);


  // FIELD 3
  // PIR motion
  // 0 = no motion
  // 1 = motion

  url += "&field3=";

  url +=
    String(
      pirDetected ? 1 : 0
    );


  // FIELD 4
  // Parking FSM state
  // 0 = VACANT
  // 1 = OCCUPIED
  // 2 = OVERTIME
  // 3 = DEPARTING
  // 4 = FAILSAFE

  url += "&field4=";

  url +=
    String(
      (int)currentState
    );


  // FIELD 5
  // Parking duration in seconds

  url += "&field5=";

  url +=
    String(parkingDuration);


  // Send HTTP request
  Serial.println();

  Serial.println(
    "Uploading to ThingSpeak..."
  );


  http.begin(url);


  int httpResponseCode =
    http.GET();


  // Check response
  if (httpResponseCode > 0) {

    String response =
      http.getString();


    Serial.print(
      "HTTP response code: "
    );

    Serial.println(
      httpResponseCode
    );


    Serial.print(
      "ThingSpeak entry ID: "
    );

    Serial.println(
      response
    );
  }


  else {

    Serial.print(
      "ThingSpeak upload error: "
    );

    Serial.println(
      httpResponseCode
    );
  }


  http.end();

  Serial.println();
}

// PARKING STATE NAME

String getStateName() {

  switch (currentState) {

    case VACANT:

      return "VACANT";


    case OCCUPIED:

      return "OCCUPIED";


    case OCCUPIED_OVERTIME:

      return "OVERTIME";


    case DEPARTING_UPLOAD:

      return "DEPARTING";


    case FAILSAFE:

      return "FAILSAFE";


    default:

      return "UNKNOWN";
  }
}