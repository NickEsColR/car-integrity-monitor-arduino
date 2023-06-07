#include "BluetoothSerial.h"
#include "ELMduino.h"
#include <AWS_IOT.h>
#include <WiFi.h>
#include <ArduinoJson.h>


BluetoothSerial SerialBT;
#define ELM_PORT   SerialBT
#define DEBUG_PORT Serial
//AWS
AWS_IOT hornbill;
int tick = 0, msgReceived = 0, msgCount = 0;
char payload[512];
char rcvdPayload[512];
StaticJsonDocument<200> doc;
char HOST_ADDRESS[]="a3u2ostlaysdce-ats.iot.us-east-1.amazonaws.com";
char CLIENT_ID[]= "ESP32test";
char PUB_TOPIC_SCANNER[]= "ESP32test/Scanner";
char PUB_TOPIC_DTC[]= "ESP32test/Anomaly";
char PUB_TOPIC_ALARM[]= "ESP32test/Alarm";
char SUB_TOPIC_GETSCANNER[] = "ESP32test/GetScanner";
char SUB_TOPIC_CHANGESENDALARM[]= "ESP32test/ChangeSendAlarm";

//ELMduino
ELM327 myELM327;
typedef enum { ENG_RPM,
               ENG_TEMP,
               SPEED,
               AIRFLOW,
               THROTLE,
               SEND} obd_pid_states;
obd_pid_states obd_state = ENG_RPM;

float rpm = 0;
float mph = 0;
float throttle = 0;
float eng_temp = 0;
float airflow = 0;
String dtc = "";
String DTC = "03\r";
bool canSendAlarm = true;
bool intruderDetected = false;

//WIFI
char WIFI_SSID[]="rzd";
char WIFI_PASSWORD[]="12345678";
int status = WL_IDLE_STATUS;

//Dual core
TaskHandle_t Task2;

//voltage para alarma
const int sensorPin = 32;   // seleccionar la entrada para el sensor
int sensorValue;      // variable que almacena el valor raw (0 a 1023)
float value;        // variable que almacena el voltaje (0.0 a 25.0)
int tickIntruder = 0;

void scanIntruder(/*void *parameter*/) {
  //for(;;){
    if(tickIntruder > 5){
      sensorValue = analogRead(sensorPin);        // realizar la lectura
      value = fmap(sensorValue, 0, 1023, 0.0, 16.5);   // cambiar escala a 0.0 - 16.5
      Serial.println(value);
      if(value >11.0 && canSendAlarm){
        Serial.println("intruso detectado");
        intruderDetected = true;
      }
      tickIntruder = 0;
    }
    tickIntruder++;
      delay(1000);  
//  }
//  vTaskDelay(10);
}
// cambio de escala entre floats
float fmap(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

////TinyGSM
//// Select your modem:
//#define TINY_GSM_MODEM_SIM800
//// Set serial for AT commands (to the module)
//#define SerialAT  Serial1
//// Your GPRS credentials, if any
//const char apn[]      = "internet.wom.co";
//const char gprsUser[] = "";
//const char gprsPass[] = "";
//#include <TinyGsmClient.h>
//TinyGsm        modem(SerialAT);
////TinyGsmClient client(modem);
//#define GSM_AUTOBAUD_MIN 9600
//#define GSM_AUTOBAUD_MAX 115200
//
//void connectGsm(){
//  // Set GSM module baud rate
//  SerialAT.begin(115200);
//  delay(6000);
//
//  // Restart takes quite some time
//  // To skip it, call init() instead of restart()
//  Serial.println("Initializing modem...");
//  modem.restart();
//
//  String modemInfo = modem.getModemInfo();
//  Serial.print("Modem Info: ");
//  Serial.println(modemInfo);
//  modem.gprsConnect(apn, gprsUser, gprsPass);
//  Serial.print("Waiting for network...");
//  if (!modem.waitForNetwork()) {
//    Serial.println(" fail");
//    delay(10000);
//    return;
//  }
//  Serial.println(" success");
//  if (modem.isNetworkConnected()) { Serial.println("Network connected"); }
//  Serial.print(F("Connecting to "));
//  Serial.print(apn);
//  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
//    Serial.println(" fail");
//    delay(10000);
//    return;
//  }
//  Serial.println(" success");
//
//  if (modem.isGprsConnected()) { Serial.println("GPRS connected"); }
//}

void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad)
{
  strncpy(rcvdPayload, payLoad, payloadLen);
  
  DeserializationError error = deserializeJson(doc, rcvdPayload);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  if (doc["message"] == "request data"){
    Serial.println("data request");
    msgReceived = 1;
  }else{
    Serial.println("change send alarm");
    canSendAlarm = !canSendAlarm;
  }
}

//void changeSendAlarmCallBackHandler (char *topicName, int payloadLen, char *payLoad)
//{
//  Serial.println("change send alarm");
//  canSendAlarm = !canSendAlarm;
//}

void connectOBD(){
  ELM_PORT.begin("ESP32test",true);
  if (!ELM_PORT.connect("DESKTOP-V51L1R3"))
  {
    DEBUG_PORT.println("Couldn't connect to OBD scanner - Phase 1");
  }

  if (!myELM327.begin(ELM_PORT, false, 2000))
  {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
  }
  Serial.println("Connected to ELM327");
}

void connectWifi(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(5000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println("Connected to wifi");
  Serial.println(WiFi.localIP());
}

void connectAWS(){
  if(hornbill.connect(HOST_ADDRESS,CLIENT_ID)== 0)
    {
        Serial.println("Connected to AWS");
        delay(1000);

        while(0!=hornbill.subscribe(SUB_TOPIC_GETSCANNER,mySubCallBackHandler))
        {
            Serial.println("Subscribe Failed, Check the Thing Name and Certificates");
            delay(500);
        }
        Serial.println("Subscribe GetScanner Successfull");

         while(0!=hornbill.subscribe(SUB_TOPIC_CHANGESENDALARM,mySubCallBackHandler))
        {
            Serial.println("Subscribe Failed, Check the Thing Name and Certificates");
            delay(500);
        }
        Serial.println("Subscribe changeSendAlarm Successfull");
    }
    else
    {
        Serial.println("AWS connection failed, Check the HOST Address");
    }
}

void setup()
{
  DEBUG_PORT.begin(115200);
  Serial.begin(115200);
  //SerialBT.setPin("1234");
    // Set GSM module baud rate
//  TinyGsmAutoBaud(SerialAT, GSM_AUTOBAUD_MIN, GSM_AUTOBAUD_MAX);
//  connectGsm();
  connectWifi();
  connectAWS();
//  connectOBD();
  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  //SerialMon.println("Initializing modem...");
  //modem.restart();

  //dual core
//  xTaskCreatePinnedToCore(
//    scanIntruder,
//    "Task_2",
//    1000,
//    NULL,
//    1,
//    &Task2,
//    0);
}

bool requestData = false;
bool dataComplete = false;

void getData(){
  switch (obd_state)
  {
    case ENG_RPM:
    {
      rpm = myELM327.rpm();
      
      if (myELM327.nb_rx_state == ELM_SUCCESS)
      {
        Serial.print("rpm: ");
        Serial.println(rpm);
        obd_state = SPEED;
      }
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
      {
        myELM327.printError();
        obd_state = SPEED;
      }
      
      break;
    }
    
    case SPEED:
    {
      mph = myELM327.mph();
      
      if (myELM327.nb_rx_state == ELM_SUCCESS)
      {
        Serial.print("mph: ");
        Serial.println(mph);
        obd_state = THROTLE;
      }
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
      {
        myELM327.printError();
        obd_state = THROTLE;
      }
      
      break;
    }
    case THROTLE:
    {
      throttle = myELM327.throttle();

      if (myELM327.nb_rx_state == ELM_SUCCESS)
      {
        Serial.print("throttle: ");
        Serial.println(throttle);
        obd_state = ENG_TEMP;
      }
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
      {
        myELM327.printError();
        obd_state = ENG_TEMP;
      }
      
      break;
    }
    case ENG_TEMP:
    {
      eng_temp = myELM327.engineCoolantTemp();

      if (myELM327.nb_rx_state == ELM_SUCCESS)
      {
        Serial.print("engine temperature: ");
        Serial.println(eng_temp);
        obd_state = AIRFLOW;
      }
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
      {
        myELM327.printError();
        obd_state = AIRFLOW;
      }
      
      break;
    }
    case AIRFLOW:
    {
      airflow = myELM327.mafRate();

      if (myELM327.nb_rx_state == ELM_SUCCESS)
      {
        Serial.print("airflow: ");
        Serial.println(airflow);
        obd_state = SEND;
      }
      else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
      {
        myELM327.printError();
        obd_state = SEND;
      }
      
      break;
    }
    case SEND:
    {
      Serial.println("data complete");
      requestData = false;
      dataComplete = true;
      handleShift();
    }
  }
}

void sendData(){
  Serial.println("Sending to aws...");
  StaticJsonDocument<200> doc;
  doc["rpm"] = rpm;
      doc["speed"] = mph;
      doc["throttle"] = throttle;
      doc["engine_temperature"] = eng_temp;
      doc["airflow"] = airflow;
  char jsonBuffer[512];
      serializeJson(doc,jsonBuffer);
      sprintf(payload, jsonBuffer);
      if (hornbill.publish(PUB_TOPIC_SCANNER, payload) == 0)
      {
        Serial.print("Publish Message:");
        Serial.println(payload);
      }
      else
      {
        Serial.println("Publish failed");
      }
      dataComplete = false;
}

bool requestDTC = false;
bool bufferEmpty = false;
bool queryReceive = false;
bool endQuery = false;
bool isCorrectResponse = false;
bool valueWritten = false;
bool dtcComplete = false;
void getDTC(){
      if (!valueWritten && bufferEmpty) {
    ELM_PORT.print(DTC);
    Serial.println("sending to OBD...");
    valueWritten = true;    
    delay(1000);
  }
  if (ELM_PORT.available()) {    
   if(bufferEmpty){
      uint8_t temp = ELM_PORT.read();
      queryReceive = true;
      endQuery = (char)temp == '>';
      if(dtc.indexOf("NO DATA")>0){
        bufferEmpty = true;
        queryReceive = false;
        endQuery = false;
        isCorrectResponse = false;
        valueWritten = false;
        Serial.println(dtc);
        tick = 0;
        dtc = "";
        handleShift();
      }
      if (dtc.endsWith("43")) {
        dtc = "";
        isCorrectResponse = true;
      }
      if(endQuery && isCorrectResponse){
        bufferEmpty = true;
        queryReceive = false;
        endQuery = false;
        isCorrectResponse = false;
        valueWritten = false;
        requestDTC = false;
        dtcComplete = true;
        tick = 0;
        handleShift();
      }else{
        dtc += (char)temp;  
      }
   }else{
    ELM_PORT.read();
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

String finaldtc = "";
void sendDTC(){
  char hex = dtc.charAt(0);
  switch (hex) {
        case '0':
            finaldtc = "P0";
            break;
        case '1':
            finaldtc = "P1";
            break;
        case '2':
            finaldtc = "P2";
            break;
        case '3':
            finaldtc = "P3";
            break;
        case '4':
            finaldtc = "C0";
            break;
        case '5':
            finaldtc = "C1";
            break;
        case '6':
            finaldtc = "C2";
            break;
        case '7':
            finaldtc = "C3";
            break;
        case '8':
            finaldtc = "B0";
            break;
        case '9':
            finaldtc = "B1";
            break;
        case 'A':
        case 'a':
            finaldtc = "B2";
            break;
        case 'B':
        case 'b':
            finaldtc = "B3";
            break;
        case 'C':
        case 'c':
            finaldtc = "U0";
            break;
        case 'D':
        case 'd':
            finaldtc = "U1";
            break;
        case 'E':
        case 'e':
            finaldtc = "U2";
            break;
        case 'F':
        case 'f':
            finaldtc = "U3";
            break;
        default:
        break;
        }
        finaldtc += dtc.substring(1,4);
        dtc = "";
        Serial.println(finaldtc);
        Serial.println("sending AWS...");
        StaticJsonDocument<200> doc;
        doc["code"] = finaldtc;
        doc["letter"] = String(finaldtc.charAt(0));
        doc["number"] = String(finaldtc.charAt(1));
        doc["subsystem_number"] = String(finaldtc.charAt(2));
        char jsonBuffer[512];
        serializeJson(doc,jsonBuffer);
        sprintf(payload, jsonBuffer);
        if (hornbill.publish(PUB_TOPIC_DTC, payload) == 0)
        {
          Serial.print("Publish Message:");
          Serial.println(payload);
        }
        else
        {
          Serial.println("Publish failed");
        }
        dtcComplete = false;
}

void sendAlarm(){
  Serial.println("sending AWS...");
        StaticJsonDocument<200> doc;
        doc["reason"] = "intruder detected";
        char jsonBuffer[512];
        serializeJson(doc,jsonBuffer);
        sprintf(payload, jsonBuffer);
        if (hornbill.publish(PUB_TOPIC_ALARM, payload) == 0)
        {
          Serial.print("Publish Message:");
          Serial.println(payload);
        }
        else
        {
          Serial.println("Publish failed");
        }
        intruderDetected = false;
}



void handleShift(){
    Serial.println("shifting BL/WIFI");
    if (WiFi.status() == WL_CONNECTED) {
      if(!intruderDetected){
        Serial.println("wifi disconnect");
        WiFi.disconnect();
        delay(1000);
        connectOBD();
      }
    } else {
      if(!intruderDetected){
        if(tick > 120){
          Serial.println("request dtc");
          tick = 0;
          requestDTC = true;
        }else{
          Serial.println("bluetooth disconnect");
          ELM_PORT.end();
          delay(1000);
          connectWifi();
          connectAWS();
        }
      }else{
        Serial.println("bluetooth disconnect");
          ELM_PORT.end();
          delay(1000);
          connectWifi();
          connectAWS();
      }
    }
}

void loop()
{
  scanIntruder();
  if(intruderDetected){
    handleShift();
    sendAlarm();
  }
  if(msgReceived == 1)
    {
        msgReceived = 0;
        requestData = true;
        Serial.println(String(requestDTC));
        handleShift();
    }
  if(tick > 120){
    handleShift();
  }
  tick++;
  delay(1000);
  if (WiFi.status() == WL_CONNECTED) {
    if(dtcComplete){
      sendDTC();
    }
    if(dataComplete){
      sendData();
    }
  }else{
      if(requestData){
       getData();
      }else{
        if(requestDTC){
          getDTC();
        }
    }
  }
}
