#include "BLEDevice.h"
#include <M5Stack.h>

#define BLYNK_NO_BUILTIN   // Disable built-in analog & digital pin operations
#define BATTERY_INTERVAL 10
// array of different LifeVit MAC addresses
char* LifeVit_DEVICES[] = {
    "f4:5e:ab:ad:62:13",  // eBlood-Pressure
};


// how often should a device be retried in a run when something fails
#define RETRY 5



// device count
static int deviceCount = sizeof LifeVit_DEVICES / sizeof LifeVit_DEVICES[0];

// the remote service we wish to connect to
static BLEUUID serviceUUID("0000fff0-0000-1000-8000-00805f9b34fb");

// the characteristic of the remote service we are interested in
static BLEUUID uuid_sensor_data("0000fff4-0000-1000-8000-00805f9b34fb");
static BLEUUID uuid_write_mode("0000fff3-0000-1000-8000-00805f9b34fb");
static BLEUUID uuid_version_battery("00002a19-0000-1000-8000-00805f9b34fb");


int diast_pressure;
int sist_pressure;
int bpm;
int battery;

//std::string sw_version;


BLEClient* getLifeVitClient(BLEAddress LifeVitAddress) {
  BLEClient* pclient = BLEDevice::createClient();

  if (!pclient->connect(LifeVitAddress)) {
    Serial.println("- Connection failed, skipping");
    return nullptr;
  }

  Serial.println("- Connection successful");
  return pclient;
}

BLERemoteService* getLifeVitService(BLEClient* pclient) {
  BLERemoteService* pService = nullptr;

  try {
   pService = pclient->getService(serviceUUID);
  }
  catch (...) {
    // something went wrong
  }
  if (pService == nullptr) {
    Serial.println("- Failed to find data service");
  }
  else {
    Serial.println("- Found data service");
  }

  return pService;
}

bool forceLifeVitServiceDataMode(BLERemoteService* pService) {
  BLERemoteCharacteristic* pCharacteristic;
  
  // get device mode characteristic, needs to be changed to read data
  Serial.println("- Force device in data mode");
  pCharacteristic = nullptr;
  try {
    pCharacteristic = pService->getCharacteristic(uuid_write_mode);
  }
  catch (...) {
    // something went wrong
  }
  if (pCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping device");
    return false;
  }

  // write the magic data
  uint8_t buf[9] = {0x02, 0x40, 0xdd, 0x0c, 0xff, 0xfd, 0x02, 0x01, 0x01};  // Data
  pCharacteristic->writeValue(buf, 9, true);

  delay(500);
  return true;
}

bool readLifeVitDataCharacteristic(BLERemoteService* pService) {
  BLERemoteCharacteristic* pCharacteristic = nullptr;

  // get the main device data characteristic
  Serial.println("- Access characteristic from device");
  try {
    pCharacteristic = pService->getCharacteristic(uuid_sensor_data);
  }
  catch (...) {
    // something went wrong
  }
  if (pCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping device");
    return false;
  }

  // read characteristic value
  Serial.println("- Read value from characteristic");
  std::string value;
  try{
    value = pCharacteristic->readValue();
  }
  catch (...) {
    // something went wrong
    Serial.println("-- Failed, skipping device");
    return false;
  }
  const char *val = value.c_str();

  Serial.print("Hex: ");
  for (int i = 0; i < 10; i++) {
    Serial.print((int)val[i], HEX);
    Serial.print(" ");
  }
  Serial.println(" ");


  diast_pressure = val[3];
  Serial.print("-- Diastolic: ");
  Serial.println(diast_pressure);

  sist_pressure = val[5];
  Serial.print("-- Systolic: ");
  Serial.println(sist_pressure);
  
 
  bpm = val[9];
  Serial.print("-- BPM: ");
  Serial.println(bpm); 
  return true;
}

bool readLifeVitBatteryCharacteristic(BLERemoteService* pService) {
  BLERemoteCharacteristic* pCharacteristic = nullptr;

  // get the device battery characteristic
  Serial.println("- Access battery characteristic from device");
  try {
    pCharacteristic = pService->getCharacteristic(uuid_version_battery);
  }
  catch (...) {
    // something went wrong
  }
  if (pCharacteristic == nullptr) {
    Serial.println("-- Failed, skipping battery level");
    return false;
  }

  // read characteristic value
  Serial.println("- Read value from characteristic");
  std::string value;
  try{
    value = pCharacteristic->readValue();
  }
  catch (...) {
    // something went wrong
    Serial.println("-- Failed, skipping battery level");
    return false;
  }
  const char *val2 = value.c_str();
  battery = val2[0];

  Serial.print("-- Battery: ");
  Serial.println(battery);


  return true;
}

bool processLifeVitService(BLERemoteService* pService, char* deviceMacAddress, bool readBattery) {
  /* set device in data mode
  if (!forceLifeVitServiceDataMode(pService)) {
    return false;
  }*/

  bool dataSuccess = readLifeVitDataCharacteristic(pService);

  bool batterySuccess = true;
  if (readBattery) {
    batterySuccess = readLifeVitBatteryCharacteristic(pService);
  }

  return dataSuccess && batterySuccess;
}

bool processLifeVitDevice(BLEAddress LifeVitAddress, char* deviceMacAddress, bool getBattery, int tryCount) {
  Serial.print("Processing LifeVit device at ");
  Serial.print(LifeVitAddress.toString().c_str());
  Serial.print(" (try ");
  Serial.print(tryCount);
  Serial.println(")");

  // connect to LifeVit ble server
  BLEClient* pclient = getLifeVitClient(LifeVitAddress);
  if (pclient == nullptr) {
    return false;
  }

  // connect data service
  BLERemoteService* pService = getLifeVitService(pclient);
  if (pService == nullptr) {
    pclient->disconnect();
    return false;
  }

  // process devices data
  bool success = processLifeVitService(pService, deviceMacAddress, getBattery);

  // disconnect from device
  pclient->disconnect();

  return success;
}



void setup() {
  // all action is done when device is woken up
  M5.begin();
  Serial.println("Starting Arduino BLE Client application for LifeVit");
  M5.Lcd.setTextSize(3);
  M5.Lcd.printf("\n\n\n\n");
  M5.Lcd.printf(" Test BLE Server LV\r\n");
  delay(1000);

 // increase boot count
  //bootCount++;


  Serial.println("Initialize BLE client...");
  BLEDevice::init("");
  //BLEDevice::setPower(ESP_PWR_LVL_P7);
  

  // check if battery status should be read - based on boot count
  //bool readBattery = ((bootCount % BATTERY_INTERVAL) == 0);
	bool readBattery = false;
  // process devices
  for (int i=0; i<deviceCount; i++) {
    int tryCount = 0;
    char* deviceMacAddress = LifeVit_DEVICES[i];
    BLEAddress LifeVitAddress(deviceMacAddress);

    while (tryCount < RETRY) {
      tryCount++;
      if (processLifeVitDevice(LifeVitAddress, deviceMacAddress, readBattery, tryCount)) {
        break;
      }
      delay(1000);
    }
    delay(1500);
  }
}
 void loop() {
  /// we're not doing anything in the loop, only on device wakeup
  delay(10000);
}
