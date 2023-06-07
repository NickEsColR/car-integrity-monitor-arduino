// This example code is in the Public Domain (or CC0 licensed, at your option.)
// By Victor Tchistiak - 2019
//
// This example demonstrates master mode Bluetooth connection to a slave BT device using PIN (password)
// defined either by String "slaveName" by default "OBDII" or by MAC address
//
// This example creates a bridge between Serial and Classical Bluetooth (SPP)
// This is an extension of the SerialToSerialBT example by Evandro Copercini - 2018
//
// DO NOT try to connect to phone or laptop - they are master
// devices, same as the ESP using this code - it will NOT work!
//
// You can try to flash a second ESP32 with the example SerialToSerialBT - it should
// automatically pair with ESP32 running this code

#include "BluetoothSerial.h"

#define USE_NAME // Comment this to use MAC address instead of a slaveName
//const char *pin = "1234"; // Change this to reflect the pin expected by the real slave BT device

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;

#ifdef USE_NAME
  String slaveName = "DESKTOP-V51L1R3"; // Change this to reflect the real name of your slave BT device
#else
  String MACadd = "28:39:26:26:DA:E0"; // This only for printing
  uint8_t address[6]  = {0x28, 0x39, 0x26, 0x26, 0xDA, 0xE0}; // Change this to reflect real MAC address of your slave BT device
#endif

String myName = "ESP32test";
bool valueWritten = false;
//OBD codes 
String RPM = "010C\r";
String ENGINE_TEMP = "0105\r";
String SPEED = "010D\r";
String AIRFLOW = "0110\r";
String THROTLE = "0111\r";
String DTC = "03\r";
String RPM_A = "0C";
String ENGINE_TEMP_A = "05";
String SPEED_A = "0D";
String AIRFLOW_A = "10";
String THROTLE_A = "11";

void setup() {
  bool connected;
  Serial.begin(38400);

  SerialBT.begin(myName, true);
  Serial.printf("The device \"%s\" started in master mode, make sure slave BT device is on!\n", myName.c_str());

  #ifndef USE_NAME
    SerialBT.setPin(pin);
    Serial.println("Using PIN");
  #endif

  // connect(address) is fast (up to 10 secs max), connect(slaveName) is slow (up to 30 secs max) as it needs
  // to resolve slaveName to address first, but it allows to connect to different devices with the same name.
  // Set CoreDebugLevel to Info to view devices Bluetooth address and device names
  #ifdef USE_NAME
    connected = SerialBT.connect(slaveName);
    Serial.printf("Connecting to slave BT device named \"%s\"\n", slaveName.c_str());
  #else
    connected = SerialBT.connect(address);
    Serial.print("Connecting to slave BT device with MAC "); Serial.println(MACadd);
  #endif

  if(connected) {
    Serial.println("Connected Successfully!");
  } else {
    while(!SerialBT.connected(10000)) {
      Serial.println("Failed to connect. Make sure remote device is available and in range, then restart app.");
    }
  }
  // Disconnect() may take up to 10 secs max
  if (SerialBT.disconnect()) {
    Serial.println("Disconnected Successfully!");
  }
  // This would reconnect to the slaveName(will use address, if resolved) or address used with connect(slaveName/address).
  SerialBT.connect();
  if(connected) {
    Serial.println("Reconnected Successfully!");
  } else {
    while(!SerialBT.connected(10000)) {
      Serial.println("Failed to reconnect. Make sure remote device is available and in range, then restart app.");
    }
  }
}



void loop() {
  getData();
}

int i = 0;
bool allDataRetrieve = false;
String rpm = "";
String e_temp = "";
String e_speed = "";
String airflow = "";
String throtle = "";

void getData(){
  if(!allDataRetrieve){
   switch (i){
      case 0:
        readECU(RPM,&rpm, RPM_A);
        break;
      case 1:
        readECU(ENGINE_TEMP,&e_temp, ENGINE_TEMP_A);
        break;
      case 2:
        readECU(SPEED,&e_speed, SPEED_A);
        break;
      case 3:
        readECU(AIRFLOW,&airflow, AIRFLOW_A);
        break;
      case 4:
        readECU(THROTLE,&throtle, THROTLE_A);
        break;
      default:
        allDataRetrieve = true;
        Serial.print("RPM: ");Serial.println(rpm);
        Serial.print("ENGINE_TEMP: ");Serial.println(e_temp);
        Serial.print("SPEED: ");Serial.println(e_speed);
        Serial.print("AIRFLOW: ");Serial.println(airflow);
        Serial.print("THROTLE: ");Serial.println(throtle);
        break;
   }
  }
}

bool queryReceive = false;
bool endQuery = false;
bool isCorrectResponse = false;

void readECU(String command,String *responsePtr,String expectedCommand) {
  if (!valueWritten) {
    SerialBT.print(command);
    Serial.println("sending...");
    valueWritten = true;    
    delay(1000);
  }
  if (SerialBT.available()) {    
    uint8_t temp = SerialBT.read();
    queryReceive = true;
    endQuery = (char)temp == '>';
    String actual = *responsePtr;
    if (actual.endsWith("41")) {
      *responsePtr = "";
      actual = "";
      temp = SerialBT.read();//first byte command answer
      actual += (char)temp;
      temp = SerialBT.read();//second byte command answer
      actual += (char)temp;
      temp = SerialBT.read();
      temp = SerialBT.read();
      if (actual == expectedCommand){
        isCorrectResponse = true;
      }
    }
    if(endQuery && isCorrectResponse){
      queryReceive = false;
      i += 1;
    }else{
      *responsePtr += (char)temp;
    }
  }else{
    if(!queryReceive){
      valueWritten = false;
      isCorrectResponse = false;
    }
  }
  delay(20);
}

bool bufferEmpty = false;
String dtc = "";

void getDTC(){
  if (!valueWritten && bufferEmpty) {
    SerialBT.print(DTC);
    Serial.println("sending...");
    valueWritten = true;    
    delay(1000);
  }
  if (SerialBT.available()) {    
   if(bufferEmpty){
      uint8_t temp = SerialBT.read();
      queryReceive = true;
      endQuery = (char)temp == '>';
      if (dtc.endsWith("43")) {
        dtc = "";
        temp = SerialBT.read();//first byte command answer
      }
      if(endQuery){
        bufferEmpty = false;
      }else{
        dtc += (char)temp;  
      }
      
   }else{
    SerialBT.read();
   }
  }else{
    bufferEmpty = true;
    if(!queryReceive){
      valueWritten = false;
      isCorrectResponse = false;
    }
  }
  delay(20);
}
