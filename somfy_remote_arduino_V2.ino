/*
 * **********************************************************************

   LIBRAIRIE declaration

 * **********************************************************************
*/
#include <FS.h>          // this needs to be first, or it all crashes and burns...
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>
#include <LiquidCrystal.h>
#include <Arduino.h>
#include <vector>

/*
 * **********************************************************************

   BOARD & PINOUT definition

 * **********************************************************************
*/
#define ESP8266 true    // define the type of board

// GPIO macros
#ifdef ESP32
#include <SPIFFS.h>
#define SIG_HIGH GPIO.out_w1ts = 1 << PORT_TX
#define SIG_LOW  GPIO.out_w1tc = 1 << PORT_TX
#elif ESP8266
#define SIG_HIGH GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << PORT_TX)
#define SIG_LOW  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << PORT_TX)
#endif
// select which pin will trigger the configuration portal when set to LOW
#define TRIGGER_PIN 0
#define PORT_TX 5 // Output data on pin 23 (can range from 0 to 31). Check pin numbering on ESP8266.

/*
 * **********************************************************************

   FILE SYSTEM & MEMORY

 * **********************************************************************
*/
// Store the rolling codes in NVS
#ifdef ESP32
#include <Preferences.h>
Preferences preferences;
#elif ESP8266
#include <EEPROM.h>
#endif

/*
 * **********************************************************************

   Global Variable & Declaration

 * **********************************************************************
*/
#include "somfy_lib.h"            // include the SOMFY remote code
int timeout = 120; // seconds to run for

// MQTT variable
WiFiClient wifiClient;
PubSubClient client(wifiClient);
char mqtt_server[40];
char mqtt_port[6]  = "1883";
//char mqtt_topic[32] = "somfy_remote";
//char mqtt_user[40] = "mqtt_user";
char mqtt_user[40];
//char mqtt_password[40] = "mqtt_password";
char mqtt_password[40];

//default custom static IP
char static_ip[16] = "192.168.1.1";
char static_gw[16] = "192.168.1.254";
char static_sn[16] = "255.255.255.0";

//flag for saving data
bool shouldSaveConfig = false;


/*
 * **********************************************************************
   Function declaration
 * **********************************************************************
*/
byte frame[7];    // Template for the compiled frame to send to RF_module
void BuildFrame(byte *frame, byte button, REMOTE remote);
void SendCommand(byte *frame, byte sync);
void receivedCallback(char* topic, byte* payload, unsigned int length);
void mqttconnect();
String getValue(String data, char separator, int index);          // ToolLib.h convertion
bool to_bool(String const& s);                                    // ToolLib.h convertion

/*
 * **********************************************************************

   SPIFF declaration - creating of the filesystem

 * **********************************************************************
*/
void setupSpiffs() {
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(mqtt_user, json["mqtt_user"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          //strcpy(mqtt_port, json["mqtt_port"]);
          //strcpy(mqtt_topic, json["mqtt_topic"]);

          // if(json["ip"]) {
          //   Serial.println("setting custom ip from config");
          //   strcpy(static_ip, json["ip"]);
          //   strcpy(static_gw, json["gateway"]);
          //   strcpy(static_sn, json["subnet"]);
          //   Serial.println(static_ip);
          // } else {
          //   Serial.println("no custom ip in config");
          // }

        }
        else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}



/*
 * **********************************************************************

   Function declaration

 * **********************************************************************
*/
void BuildFrame(byte *frame, byte button, REMOTE remote);
void SendCommand(byte *frame, byte sync);
void receivedCallback(char* topic, byte* payload, unsigned int length);
void mqttconnect();

/*
 * **********************************************************************

   Initialisation of the BOARD

 * **********************************************************************
*/
void setup() {
  Serial.begin(115200);                 // Define the speed of USB Serial port
  Serial.println();

  pinMode(TRIGGER_PIN, INPUT);          // Button for Configuration Portail
  pinMode(PORT_TX, OUTPUT);             // Output to somfy 433.42MHz transmitter
  SIG_LOW;

  setupSpiffs();                        // Initialisation of the FileSystem

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  // setup custom parameters
  //
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 40);
  //WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", "", 32);

  //add all your parameters here
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_password);
  //wm.addParameter(&custom_mqtt_topic);

  // set static ip
  // IPAddress _ip,_gw,_sn;
  // _ip.fromString(static_ip);
  // _gw.fromString(static_gw);
  // _sn.fromString(static_sn);
  // wm.setSTAStaticIPConfig(_ip, _gw, _sn);

  //reset settings - wipe credentials for testing
  //wm.resetSettings();

  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name
  //here  "AutoConnectAP" if empty will auto generate basedcon chipid, if password is blank it will be anonymous
  //and goes into a blocking loop awaiting configuration
  if (!wm.autoConnect("somfy_remote_portail")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // if we still have not connected restart and try all over again
    ESP.restart();
    delay(5000);
  }

  // always start configportal for a little while
  wm.setConfigPortalTimeout(60);
  //wm.startConfigPortal("somfy_remote_portail","password");
  wm.startConfigPortal("somfy_remote_portail");     // no wifi password

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  //strcpy(mqtt_topic, custom_mqtt_topic.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"]     = mqtt_server;
    json["mqtt_port"]       = mqtt_port;
    json["mqtt_user"]       = mqtt_user;
    json["mqtt_password"]   = mqtt_password;
    //json["mqtt_topic"]   = mqtt_topic;

    // json["ip"]          = WiFi.localIP().toString();
    // json["gateway"]     = WiFi.gatewayIP().toString();
    // json["subnet"]      = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

  Serial.print("IP_adress :");
  Serial.println(WiFi.localIP());
  Serial.print("IP_gateway :");
  Serial.println(WiFi.gatewayIP());
  Serial.print("IP_mask:");
  Serial.println(WiFi.subnetMask());

  // Configure MQTT
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(receivedCallback);

  // Open storage for storing the rolling codes
#ifdef ESP32
  preferences.begin("somfy-remote", false);
#elif ESP8266
  EEPROM.begin(1024);
#endif

  // Clear all the stored rolling codes (not used during normal operation). Only ESP32 here (ESP8266 further below).
#ifdef ESP32
  if ( reset_rolling_codes ) {
    preferences.clear();
  }
#endif

  // Print out all the configured remotes.
  // Also reset the rolling codes for ESP8266 if needed.
  for ( REMOTE remote : remotes ) {
    Serial.print("Simulated remote number : ");
    Serial.println(remote.id, HEX);
    Serial.print("Current rolling code    : ");
    unsigned int current_code;

#ifdef ESP32
    current_code = preferences.getUInt( (String(remote.id) + "rolling").c_str(), remote.default_rolling_code);
#elif ESP8266
    if ( reset_rolling_codes ) {
      EEPROM.put( remote.eeprom_address, remote.default_rolling_code );
      EEPROM.commit();
    }
    EEPROM.get( remote.eeprom_address, current_code );
    // Check if the EEprom value is compliant with somfy protocol, range [0 to 0xFFFF]
    // If not, the code will be reset and fix to 0
    if (current_code > 65535) {
      current_code = remote.default_rolling_code;
      EEPROM.put( remote.eeprom_address, remote.default_rolling_code );
      EEPROM.commit();
    }
#endif

    Serial.println( current_code );
  }
  Serial.println();
}

/*
 * **********************************************************************

   MAIN LOOP

 * **********************************************************************
*/
void loop() {
  // put your main code here, to run repeatedly:
  // is configuration portal requested?
  if ( digitalRead(TRIGGER_PIN) == LOW) {
    ESP.restart();      // Restart the ESP and Open the portail for 60sec
    /*
      WiFiManager wm;

      //reset settings - for testing
      //wifiManager.resetSettings();

      // set configportal timeout
      wm.setConfigPortalTimeout(timeout);

      // If the button is pressed, opening the portail for configuration
      if (!wm.startConfigPortal("somfy_remote_portail")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
      }
    */
  }
  // Reconnect MQTT if needed
  if ( !client.connected() ) {
    mqttconnect();
  }

  client.loop();

  delay(100);
}

/*
 * ********************************************************************************************
   MQTT connection & callback
 * ********************************************************************************************
*/
void mqttconnect() {
  String strCurrent_code = "";
  // Loop until reconnected
  while ( !client.connected() ) {
    Serial.print("MQTT connecting ...");

    // Connect to MQTT, with retained last will message "offline"
    if (client.connect(mqtt_id, mqtt_user, mqtt_password, status_topic, 1, 1, "offline")) {
      Serial.println("connected");

      // Subscribe to the topic of each remote with QoS 1
      for ( REMOTE remote : remotes ) {
        client.subscribe(remote.mqtt_topic, 1);
        Serial.print("Subscribed to topic: ");
        Serial.println(remote.mqtt_topic);
      }
      //
      // Update status, message is retained
      // - post on the MQTT server the help and all topics available
      //
      client.publish(status_topic, "online", true);
      client.publish(help_topic, "list of commands: up, down, stop, prog, reboot, reset_code, read_code, write_code 0 to 65534", true);

      // Publish the list of all device available
      // Show the remode ID
      // Show the current rooling code
      char mqtt_buffer_topic[48] = "";
      for ( REMOTE remote : remotes ) {
        strcpy(mqtt_buffer_topic, remote.mqtt_topic);    // Record the mqtt topic
        // Need to clear the last order to not redo the last action after Reboot.
        // Issue was to send a RF to all device after each disconnection or reboot
        client.publish(mqtt_buffer_topic, "ready", true);          // Erase the last order and put READY after boot
        strcat(mqtt_buffer_topic, "/state");
        client.publish(mqtt_buffer_topic, "NA", true);          // Define the state as undefined after boot

        // send the remote ID
        strcpy(mqtt_buffer_topic, remote.mqtt_topic);    // Record the mqtt topic
        strcat(mqtt_buffer_topic, "/ID");
        strCurrent_code =  String(remote.id);
        client.publish(mqtt_buffer_topic, strCurrent_code.c_str() , true);         // send the ID

        // Send the current code on the Topic
        strcpy(mqtt_buffer_topic, remote.mqtt_topic);    // Record the mqtt topic
        strcat(mqtt_buffer_topic, "/code");
        strCurrent_code =  String(EEPROM.read(remote.eeprom_address));
        client.publish(mqtt_buffer_topic, strCurrent_code.c_str() , true);         // send the RollingCode
      }
    }
    else {
      Serial.print("failed, status code =");
      Serial.println(client.state());
      Serial.print("MQTT server=");
      Serial.println(mqtt_server);
      Serial.print("MQTT port=");
      Serial.println(mqtt_port);

      Serial.print("MQTT user=");
      Serial.println(mqtt_user);
      Serial.print("MQTT password=");
      Serial.println(mqtt_password);
      Serial.print("MQTT topic=");
      Serial.println(mqtt_id);
      
      Serial.println("try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      return;   // exit from MQTT connect and go back on LOOP function
    }
    if ( digitalRead(TRIGGER_PIN) == LOW) {
      ESP.restart();      // Restart the ESP and Open the portail for 60sec
    }
  }
}
/*
 * ********************************************************************************************
   Action when a MQTT is received
 * ********************************************************************************************
*/
void receivedCallback(char* topic, byte* payload, unsigned int length) {
  char const* state_shutter;
  bool commandIsValid = false;
  bool commandSuccess = false;
  uint16_t current_code;
  REMOTE currentRemote;
  payload[length] = '\0';                               // need to add symbol end of string to work
  String strPayload = String((char*)payload);           // Convert Payload to string
  char mqtt_buffer_topic[48] = "";
  String strCurrent_code = "";

  Serial.print("\nMQTT message received: ");
  Serial.println(topic);

  Serial.print("Payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Command is valid if the payload contains one of the chars below AND the topic corresponds to one of the remote
  if ( length > 0  ) {

    // select the remote based on the topic
    for ( REMOTE remote : remotes ) {
      if ( strcmp(remote.mqtt_topic, topic) == 0 ) {
        currentRemote = remote;     // Save the topic
      }
    }

    String strCommand = getValue(strPayload, ' ', 0);       // convert the string into Command & Data with ' ' separator
    String strData = getValue(strPayload, ' ', 1);

    if ( strCommand == "up" || strCommand == "stop" || strCommand == "down" || strCommand == "prog" ) {
      commandIsValid = true;
      commandSuccess = true;
    }
    else if ( strCommand == "reset_code") {
      Serial.println("Erasing rolling code");
      Serial.print("Simulated remote number : 0x");
      Serial.println(currentRemote.id, HEX);
      Serial.print("Current rolling code    : ");
      EEPROM.get( currentRemote.eeprom_address, current_code );
      Serial.println(current_code);
#ifdef ESP32
      current_code = preferences.getUInt( (String(remote.id) + "rolling").c_str(), remote.default_rolling_code);
#elif ESP8266
      current_code = currentRemote.default_rolling_code;    // Use the default value
      EEPROM.put( currentRemote.eeprom_address, currentRemote.default_rolling_code );
      EEPROM.commit();
#endif
      Serial.print("New rolling code    : ");
      Serial.println( current_code );
      commandSuccess = true;
    }
    else if ( strCommand == "reboot") {
      Serial.println("Reset..");
      ESP.restart();
    }
    else if ( strCommand == "read_code") {
      Serial.println("Read rolling code");
      Serial.print("Simulated remote number : 0x");
      Serial.println(currentRemote.id, HEX);
      Serial.print("Current rolling code    : ");
      EEPROM.get( currentRemote.eeprom_address, current_code );
      Serial.println(current_code);
      // Send the current code on the Topic
      strcpy(mqtt_buffer_topic, currentRemote.mqtt_topic);
      strcat(mqtt_buffer_topic, "/code");
      strCurrent_code.concat( String(EEPROM.read(currentRemote.eeprom_address)));
      client.publish(mqtt_buffer_topic, strCurrent_code.c_str() , true);
      // done
      commandSuccess = true;
    }
    else if ( strCommand == "write_code") {
      //
      // Rolling code can be reset to the default value
      // Show the old and the new code value
      //

      Serial.println("Write new code");
      Serial.print("command = ");
      Serial.println(strCommand);
      if (strData != "") {                                // if data != null
        int newcode = strData.toInt();        // convert string to int
        if (newcode >= 0 && newcode < 65535) {
          Serial.print("Simulated remote number : 0x");
          Serial.println(currentRemote.id, HEX);
          Serial.print("Current rolling code    : ");
          EEPROM.get( currentRemote.eeprom_address, current_code );
          Serial.println(current_code);
#ifdef ESP32
          current_code = preferences.getUInt( (String(remote.id) + "rolling").c_str(), remote.default_rolling_code);
#elif ESP8266
          EEPROM.put( currentRemote.eeprom_address, newcode );
          EEPROM.commit();
#endif
          // Send the current code on the Topic
          strcpy(mqtt_buffer_topic, currentRemote.mqtt_topic);
          strcat(mqtt_buffer_topic, "/code");
          strCurrent_code.concat( String(EEPROM.read(currentRemote.eeprom_address)));
          client.publish(mqtt_buffer_topic, strCurrent_code.c_str() , true);

          Serial.print("New rolling code    : ");
          Serial.println( newcode );
          commandSuccess = true;
        }
        else {
          Serial.println("new_code is invalid : Range [0 , 65534]");
        }
      }

      else {
      }
    }
    // If the command is unknown
    else {
      return;
    }
    // Erase the last topic
    if (commandSuccess) {
      client.publish(currentRemote.mqtt_topic, "Success", true);          // Erase the last order and put READY after boot
    }
    else {
      client.publish(currentRemote.mqtt_topic, "Command Error", true);          // Erase the last order and put READY after boot
    }
  }
  if ( commandIsValid ) {
    if ( strPayload == "up" ) {
      Serial.println("Open");
      BuildFrame(frame, BUTTON_UP, currentRemote);
      state_shutter = "open";
    }
    else if ( strPayload == "stop" ) {
      Serial.println("Stop");
      BuildFrame(frame, BUTTON_MY, currentRemote);
      state_shutter = "stop";
    }
    else if ( strPayload == "down" ) {
      Serial.println("Close");
      BuildFrame(frame, BUTTON_DOWN, currentRemote);
      state_shutter = "closed";
    }
    else if ( strPayload == "prog" ) {
      Serial.println("Prog");
      BuildFrame(frame, PROG, currentRemote);
      state_shutter = "program";
    }

    Serial.println("");

    SendCommand(frame, 2);
    for ( int i = 0; i < 2; i++ ) {
      SendCommand(frame, 7);
    }

    // Send the MQTT ack message
    String ackString = "id: 0x";
    ackString.concat( String(currentRemote.id, HEX) );
    ackString.concat(", cmd: ");
    ackString.concat(strPayload);
    ackString.concat(", code: ");
    ackString.concat( String(EEPROM.read(currentRemote.eeprom_address)));

    // Create the MQTT based on the device name

    strcpy(mqtt_buffer_topic, currentRemote.mqtt_topic);
    strcat(mqtt_buffer_topic, "/code");

    strCurrent_code.concat( String(EEPROM.read(currentRemote.eeprom_address)));
    client.publish(mqtt_buffer_topic, strCurrent_code.c_str() , true);

    strcpy(mqtt_buffer_topic, currentRemote.mqtt_topic);
    strcat(mqtt_buffer_topic, "/ack");
    client.publish(mqtt_buffer_topic, ackString.c_str());

    strcpy(mqtt_buffer_topic, currentRemote.mqtt_topic);
    strcat(mqtt_buffer_topic, "/state");
    client.publish(mqtt_buffer_topic, state_shutter, true);

  }
}
/*
 * **********************************************************************

   SOMFY LIBRAIRIE

 * **********************************************************************
*/

/*
* ********************************************************************************************
  Creation of the frame to send to emulate the somfy remote
* ********************************************************************************************
*/
void BuildFrame(byte *frame, byte button, REMOTE remote) {
  //unsigned int code;
  uint16_t code;            // Rolling code is 16bits but UINT on ESP is 32bit long so need to fix the sizeof code

#ifdef ESP32
  code = preferences.getUInt( (String(remote.id) + "rolling").c_str(), remote.default_rolling_code);
#elif ESP8266
  EEPROM.get( remote.eeprom_address, code );
#endif
  frame[0] = 0xA7;            // Encryption key. Doesn't matter much
  frame[1] = button << 4;     // Which button did  you press? The 4 LSB will be the checksum
  frame[2] = code >> 8;       // Rolling code (big endian)
  frame[3] = code;            // Rolling code
  frame[4] = remote.id >> 16; // Remote address
  frame[5] = remote.id >>  8; // Remote address
  frame[6] = remote.id;       // Remote address

  Serial.print("Frame         : ");
  for (byte i = 0; i < 7; i++) {
    if (frame[i] >> 4 == 0) { //  Displays leading zero in case the most significant nibble is a 0.
      Serial.print("0");
    }
    Serial.print(frame[i], HEX); Serial.print(" ");
  }

  // Checksum calculation: a XOR of all the nibbles
  byte checksum = 0;
  for (byte i = 0; i < 7; i++) {
    checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
  }
  checksum &= 0b1111; // We keep the last 4 bits only


  // Checksum integration
  frame[1] |= checksum; //  If a XOR of all the nibbles is equal to 0, the blinds will consider the checksum ok.

  Serial.println(""); Serial.print("With checksum : ");
  for (byte i = 0; i < 7; i++) {
    if (frame[i] >> 4 == 0) {
      Serial.print("0");
    }
    Serial.print(frame[i], HEX); Serial.print(" ");
  }


  // Obfuscation: a XOR of all the bytes
  for (byte i = 1; i < 7; i++) {
    frame[i] ^= frame[i - 1];
  }

  Serial.println(""); Serial.print("Obfuscated    : ");
  for (byte i = 0; i < 7; i++) {
    if (frame[i] >> 4 == 0) {
      Serial.print("0");
    }
    Serial.print(frame[i], HEX); Serial.print(" ");
  }
  Serial.println("");
  Serial.print("Rolling Code  : ");
  Serial.println(code);

#ifdef ESP32
  preferences.putUInt( (String(remote.id) + "rolling").c_str(), code + 1); // Increment and store the rolling code
#elif ESP8266
  EEPROM.put( remote.eeprom_address, code + 1 );
  EEPROM.commit();      // Force to execute the EEPROM write
#endif
}

/*
 * ********************************************************************************************
   SOMFY protocol to send the message
 * ********************************************************************************************
*/
void SendCommand(byte *frame, byte sync) {
  if (sync == 2) { // Only with the first frame.
    //Wake-up pulse & Silence
    SIG_HIGH;
    delayMicroseconds(9415);
    SIG_LOW;
    delayMicroseconds(89565);
  }

  // Hardware sync: two sync for the first frame, seven for the following ones.
  for (int i = 0; i < sync; i++) {
    SIG_HIGH;
    delayMicroseconds(4 * SYMBOL);
    SIG_LOW;
    delayMicroseconds(4 * SYMBOL);
  }

  // Software sync
  SIG_HIGH;
  delayMicroseconds(4550);
  SIG_LOW;
  delayMicroseconds(SYMBOL);

  //Data: bits are sent one by one, starting with the MSB.
  for (byte i = 0; i < 56; i++) {
    if (((frame[i / 8] >> (7 - (i % 8))) & 1) == 1) {
      SIG_LOW;
      delayMicroseconds(SYMBOL);
      SIG_HIGH;
      delayMicroseconds(SYMBOL);
    }
    else {
      SIG_HIGH;
      delayMicroseconds(SYMBOL);
      SIG_LOW;
      delayMicroseconds(SYMBOL);
    }
  }

  SIG_LOW;
  delayMicroseconds(30415); // Inter-frame silence
}

/*
 * ********************************************************************************************
    EXTRA function from toolLib.h
 * ********************************************************************************************
*/
bool to_bool(String const& s) { // thanks Chris Jester-Young from stackoverflow
  return s != "0";
}

// Function to split a string into 2 or more string based on charactere. Equivalent to strtok with String
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
