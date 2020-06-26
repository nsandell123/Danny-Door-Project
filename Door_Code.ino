#include <ESP8266WiFi.h> // enabling wifi
#include <ESP8266mDNS.h> // mdns
#include <ESP8266WebServer.h> // webserver
#include "PageBuilder.h" // dynamic page construction
#include <AutoConnect.h> // captive portal - wifi config page door.local/_ac
#include <Wire.h> // motor shield
#include <Adafruit_MotorShield.h> // motor shield
// #define AC_DEBUG

// initialize the motor shield
Adafruit_MotorShield AFMS = Adafruit_MotorShield();

// Assign the motor port on the shield
Adafruit_DCMotor *myMotor = AFMS.getMotor(3);

// hard-coded value (travel time of door) (Milliseconds = runtime*10);
const int runtime = 1300;

// HTML of home page
static const char PROGMEM _PAGE_DOOR[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1">
  <title>Door Control</title>
  <style type="text/css">
  {{STYLE}}
  </style>
</head>
<body>
  <p>Door Control</p>
  <div class="one">
    <p><a class="button" href="/?door=open">OPEN</a></p>
    <p><a class="button" href="/?door=close">CLOSE</a></p>
    <!-- <p><a class="button" href="/setup">Calibrate</a></p> -->
    <p>Status: {{DOORIO}}</p>
  </div>
</body>
</html>
)rawliteral";

// styles for Web page. A style description can be separated from the page structure.
// It expands from {{STYLE}} caused by declaration of 'Button' PageElement.
static const char PROGMEM _STYLE_BUTTON[] = R"rawliteral(
body {-webkit-appearance:none;}
p {
  font-family:'Arial',sans-serif;
  font-weight:bold;
  text-align:center;
}
.button {
  display:block;
  width:150px;
  margin:10px auto;
  padding:7px 13px;
  text-align:center;
  background:#668ad8;
  font-size:20px;
  color:#ffffff;
  white-space:nowrap;
  box-sizing:border-box;
  -webkit-box-sizing:border-box;
  -moz-box-sizing:border-box;
}
.button:active {
  font-weight:bold;
  vertical-align:top;
  padding:8px 13px 6px;
}
.one a {text-decoration:none;}
)rawliteral";

// ONBOARD_LED is WiFi connection indicator.
// BUILTIN_LED is controled by the web page.
#define ONBOARD_LED 2     // Different pin assignment by each esp8266 module

// Handler for buttons
String doorIO(PageArgument& args) {
  if (args.hasArg("door")) {
    if (args.arg("door") == "open") {
        Serial.println("opening");
        runMotor(1);
    myMotor->run(RELEASE);
    } else if (args.arg("door") == "close") {
        Serial.println("closing");
        runMotor(0);
    } 
//    else if (args.arg("door") == "stop"){
//        myMotor->run(RELEASE);
//    }
    return "Moved";
  } else {
    return "Ready";
  }
  // stop running this program
  // the entire board sleeps for 10 ms. Sleep only 10ms so it does not disconnect wifi
  delay(10);
}

// Motor drive function
void runMotor(int state) {
  if(state == 1) {
    myMotor->run(FORWARD);
    for (int i = 0; i < runtime; i++) {
      // delay motor control (arduino process)
      yield();
      // the entire board sleeps for 10 ms. Sleep only 10ms so it does not disconnect wifi
      delay(10);
    }
    // stops getting the power to the motor
    myMotor->run(RELEASE);
  } else {
    myMotor->run(BACKWARD);
    for (int i = 0; i < runtime; i++) {
      // delay motor control (arduino process)
      yield();
      // the entire board sleeps for 10 ms. Sleep only 10ms so it does not disconnect wifi
      delay(10);
    }
    // stops getting the power to the motor
    myMotor->run(RELEASE);

  }
}

// Architecture of chip
String getArch(PageArgument& args) {
  return "ESP8266";
}

// Page construction
PageElement Button(_PAGE_DOOR, {
  {"STYLE", [](PageArgument& arg) { return String(_STYLE_BUTTON); }},
  {"ARCH", getArch},
  {"DOORIO", doorIO}
});
PageBuilder DOORPage("/", {Button});

//static const char PROGMEM _SETUP_PAGE[] = R"rawliteral(
//<!DOCTYPE html>
//<html>
//<head>
//  <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1">
//  <title>Door Control</title>
//  <style type="text/css">
//  {{STYLE}}
//  </style>
//</head>
//<body>
//  <p>Calibration</p>
//  <div class="one">
//    <p><a class="button" href="/">BACK</a></p>
//  </div>
//</body>
//</html>
//)rawliteral";
//PageElement SETUPPage_ELEMENT(_SETUP_PAGE, {
//  {"STYLE", [](PageArgument& arg) { return String(_STYLE_BUTTON); }},
//  {"ARCH", getArch}
//});
//PageBuilder SETUPPage("/setup", {SETUPPage_ELEMENT});

PageElement NOTFOUND_PAGE_ELEMENT("<p style=\"font-size:36px;color:red;\">Woops!</p><p>404 - Page not found.</p>");
PageBuilder NOTFOUND_PAGE({NOTFOUND_PAGE_ELEMENT});

// Server initialization
ESP8266WebServer  Server;
// for captive portal
AutoConnect Portal(Server);
AutoConnectConfig Config;

//const char* SSID = "ATT2CnX6M2";  // Modify for your available WiFi AP
//const char* PSK  = "9ir#xy8if4y7";  // Modify for your available WiFi AP

// Init
void setup() {
  // Turn on the chip for the first time and wait for everything to turn on
  delay(1000);  // for stable operation of the module.
  Serial.begin(9600); // starting debugging monitor at 9600 bauds

  pinMode(ONBOARD_LED, OUTPUT); // setting the pin to either be input or output
  digitalWrite(ONBOARD_LED, HIGH); // power on the pin for onboard LED
  pinMode(BUILTIN_LED, OUTPUT); // setting the pin to either be input or output
  digitalWrite(BUILTIN_LED, HIGH); // same function as above
  
  DOORPage.insert(Server); // insert the root page we constructed into web server
//  SETUPPage.insert(Server);
  NOTFOUND_PAGE.atNotFound(Server);    // Add not found page
  // using AutoConnectConfig
  Config.apid = "door";
  Config.hostName = "door";
  Config.title = "DoorControl";
  WiFi.hostname("door");
  Portal.config(Config);
  if (Portal.begin()) {
    Serial.println("HTTP server:" + WiFi.localIP().toString());
    if (MDNS.begin("door")) {              // Start the mDNS responder for door.local
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS responder started at door.local");
    } else {
      Serial.println("Error setting up MDNS responder!");
    }
  }
  Serial.println("web server started: " + WiFi.localIP().toString());

  // Starting Motor Shield
  AFMS.begin();
  myMotor->setSpeed(255);
  myMotor->run(FORWARD);
  // turn on motor
  delay(1);
  myMotor->run(RELEASE);
}

// Continuous Handling
void loop() {
  if (WiFi.status() != WL_CONNECTED)
    digitalWrite(ONBOARD_LED, LOW);
  else
    digitalWrite(ONBOARD_LED, HIGH);
  Portal.handleClient();
  MDNS.update();

// Test run motors
//  myMotor->run(BACKWARD);
//  for (int i = 0; i < 500; i++) {
//    yield();
//    delay(10);
//  }
//  myMotor->run(RELEASE);
//
//  Serial.print("tock");
//
//  myMotor->run(FORWARD);
//  for (int i = 0; i < 500; i++) {
//    yield();
//    delay(10);
//  }
//  myMotor->run(RELEASE);

}
