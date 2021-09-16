// You can add as many remote control emulators as you want by adding elements to the "remotes" vector
// The id and mqtt_topic can have any value but must be unique
// default_rolling_code can be any unsigned int, usually leave it at 1
// eeprom_address must be incremented by 4 for each remote

// Once the programe is uploaded on the ESP32:
// - Long-press the program button of YOUR ACTUAL REMOTE until your blind goes up and down slightly
// - send 'p' using MQTT on the corresponding topic
// - You can use the same remote control emulator for multiple blinds, just repeat these steps.
//
// Then:
// - up     will make it go up
// - stop   make it stop
// - down   will make it go down
// - prog   to program the remote with the somfy device
//    

// Change reset_rolling_codes to true to clear the rolling codes stored in the non-volatile storage
// The default_rolling_code will be used


const char*    mqtt_id = "somfy_remote";
const char*    status_topic = "somfy_remote/status";           // Online / offline
const char*    help_topic = "somfy_remote/help";               // Put the help on this topic

// Configuration of the remotes that will be emulated
struct REMOTE {
  unsigned int id;
  char const* mqtt_topic;
  unsigned int default_rolling_code;
  uint32_t eeprom_address;
};
//                                 id            mqtt_topic     default_rolling_code     eeprom_address
// rulling code is a 32bits and EEprom data is 8bits so need 4 adress for 1 code
std::vector<REMOTE> const remotes = {{0x194623, "somfy_remote/all", 1, 0}
                                    ,{0x194624, "somfy_remote/device1", 1, 4}
                                    ,{0x194625, "somfy_remote/device2", 1, 8}
                                    ,{0x194626, "somfy_remote/device3", 1, 12}
                                    ,{0x194627, "somfy_remote/device4", 1, 16}
                                    ,{0x194628, "somfy_remote/device5", 1, 20}
                                    ,{0x194629, "somfy_remote/device6", 1, 24}
                                    ,{0x194630, "somfy_remote/device7", 1, 28}
                                    ,{0x194631, "somfy_remote/device8", 1, 32}
                                    ,{0x194632, "somfy_remote/device9", 1, 36}
                                    ,{0x194633, "somfy_remote/device10", 1, 40}
                                    ,{0x194634, "somfy_remote/device11", 1, 44}
                                    ,{0x194635, "somfy_remote/device12", 1, 48}
                                    ,{0x194636, "somfy_remote/device13", 1, 52}
                                    ,{0x194637, "somfy_remote/device14", 1, 56}
                                    ,{0x194638, "somfy_remote/device15", 1, 60}
                                    ,{0x194639, "somfy_remote/device16", 1, 64}
                                    };
const bool reset_rolling_codes = false;

// Buttons & SOMFY command available
#define SYMBOL 640
#define BUTTON_MY 0x1
#define BUTTON_UP 0x2
#define BUTTON_MY_UP 0x3
#define BUTTON_DOWN 0x4
#define BUTTON_MY_DOWN 0x5
#define BUTTON_UP_DOWN 0x6
#define PROG 0x8
#define SUNFLAG_ENABLE_DETECTOR 0x9
#define SUNFLAG_DISABLE_DETECTOR 0xA
