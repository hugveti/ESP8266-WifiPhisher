/*  
  * ESP8266 WifiPhisher
  * https://github.com/244v234/ESP8266-WifiPhisher
  * https://hackster.io.com/244v234
  * http://wifiphisher.tk
 */

extern "C" {
#include "user_interface.h"
}

#include <EEPROM.h>
#include <ArduinoJson.h>
#if ARDUINOJSON_VERSION_MAJOR != 5
#error Please upgrade/downgrade ArduinoJSON library to version 5!
#endif

#include "A_config.h"
#include "Attack.h"
#include "CLI.h"
#include "Credential.h"
#include "Alert.h"
#include "DeviceSleep.h"
#include "DisplayUI.h"
#include "Keyboard.h"
#include "Names.h"
#include "SSIDs.h"
#include "Scan.h"
#include "Settings.h"
#include "WifiConfigData.h"
#include "functions.h"
#include "language.h"
#include "oui.h"
#include "webfiles.h"
#include <MD5Builder.h>

Settings settings;
Names names;
SSIDs ssids;
Accesspoints accesspoints;
Stations stations;
Scan scan;
Attack attack;
CLI cli;
DisplayUI displayUI;
Credential credential;
WifiConfigData wifiConfig;

Alert alert;
Keyboard keyboard;
DeviceSleep deviceSleep;
#include "wifi.h"

uint32_t autosaveTime = 0;
uint32_t currentTime = 0;

bool booted = false;

MD5Builder _md5;

String md5(String str) {
  _md5.begin();
  _md5.add(String(str));
  _md5.calculate();
  return _md5.toString();
}

void setup() {
  // for random generator
  randomSeed(os_random());

  // start serial
  Serial.begin(115200);
  Serial.println();
  // Start EEPROM
  EEPROM.begin(4096);

  // auto repair when in boot-loop
  uint8_t bootCounter = EEPROM.read(0);

  if (bootCounter >= 5) {
    SPIFFS.format();
  } else {
    EEPROM.write(0, bootCounter + 1); // add 1 to the boot counter
    EEPROM.commit();
  }

  // get time
  currentTime = millis();

  // load settings
  settings.load();

  // set mac for access point
  wifi_set_macaddr(SOFTAP_IF, settings.getMacAP());

  // start WiFi //help //WIFI_AP
  wifi_set_opmode(SOFTAP_MODE);
  wifi_set_promiscuous_rx_cb(
      [](uint8_t *buf, uint16_t len) { scan.sniffer(buf, len); });

  // set mac for station
  wifi_set_macaddr(STATION_IF, settings.getMacSt());

  // start display

  if (settings.getDisplayInterface()) {
    // displayUI.initLcd();
    displayUI.setup();
    displayUI.mode = displayUI.DISPLAY_MODE::INTRO;
  }

  names.load();
  ssids.load();
  cli.load();
  keyboard.init();
  wifiConfig.init();
  credential.init();
  alert.setupButton();
  deviceSleep.setup();
  credential.toSerial();
  wifiConfig.toSerial();

  // create scan.json
  scan.setup();

  // alert.init();

  // set channel
  setWifiChannel(settings.getChannel());

// load Wifi settings: SSID, password,...
#ifdef DEFAULT_SSID
  if (settings.getSSID() == "pwned")
    settings.setSSID(DEFAULT_SSID);
#endif // ifdef DEFAULT_SSID
  loadWifiConfigDefaults();
  // dis/enable serial command interface
  if (settings.getCLI()) {
    cli.enable();
  } else {
    Serial.flush();
    Serial.end();
  }
  // start access point/web interface
  if (settings.getWebInterface()) {
    startAP();
  }
pinMode(D4, OUTPUT);
}
unsigned long cnt = 0;

void loop() {
  cnt = cnt + 1;
  currentTime = millis();
  wifiUpdate(); // manage access point
  attack.update();
  displayUI.update();
  cli.update();   // read and run serial input
  scan.update();  // run scan
  ssids.update(); // run random mode, if enabled
  if (settings.getAutosave() &&
      (currentTime - autosaveTime > settings.getAutosaveTime())) {
    autosaveTime = currentTime;
    names.save(false);
    ssids.save(false);
    settings.save(false);
  }

  if (!booted) {
    // reset boot counter
    EEPROM.write(0, 0);
    EEPROM.commit();
    booted = true;
    digitalWrite(D4, HIGH);
  }
  if(digitalRead(12) == LOW && digitalRead(14) == LOW) {
    alert.showSuccess(str(D_DEEP_SLEEP));
    displayUI.off();
    deviceSleep.sleep();
  }
}
