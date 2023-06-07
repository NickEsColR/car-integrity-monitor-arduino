#include "BluetoothSerial.h"
#include "ELMduino.h"
//#include <TinyGsmClient.h>
#include <AWS_IOT.h>
#include <ArduinoJson.h>
#include <WiFi.h>

BluetoothSerial SerialBT;
#define ELM_PORT   SerialBT
#define DEBUG_PORT Serial
//AWS
AWS_IOT hornbill;
int tick = 0, msgReceived = 0, msgCount = 0;
char payload[512];
char rcvdPayload[512];
StaticJsonDocument<200> doc;
char HOST_ADDRESS[]="AWS host address";
char CLIENT_ID[]= "client id";
char TOPIC_NAME[]= "your thing/topic name";
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

//WIFI
char WIFI_SSID[]="rzd";
char WIFI_PASSWORD[]="12345678";
int status = WL_IDLE_STATUS;

// TTGO T-Call pins
//#define MODEM_RST            5
//#define MODEM_PWKEY          4
//#define MODEM_POWER_ON       23
//#define MODEM_TX             27
//#define MODEM_RX             26
//#define I2C_SDA              21
//#define I2C_SCL              22

// Configure TinyGSM library
//#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
//#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

// Set serial for AT commands (to SIM800 module)
//#define SerialAT Serial1
//TinyGsm modem(SerialAT);

// TinyGSM Client for Internet connection
//TinyGsmClient client(modem);



void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad)
{
  strncpy(rcvdPayload, payLoad, payloadLen);
  //rcvdPayload[payloadLen] = 0;
  msgReceived = 1;
  DeserializationError error = deserializeJson(doc, rcvdPayload);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
}

void connectOBD(){
  ELM_PORT.begin("ESP32test",true);
  if (!ELM_PORT.connect("DESKTOP-V51L1R3"))
  {
    DEBUG_PORT.println("Couldn't connect to OBD scanner - Phase 1");
    while(1);
  }

  if (!myELM327.begin(ELM_PORT, false, 2000))
  {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
    while (1);
  }
}

void connectWfi(){
  while (status != WL_CONNECTED)
    {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(WIFI_SSID);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        // wait 5 seconds for connection:
        delay(5000);
    }

    Serial.println("Connected to wifi");
}

void connectAWS(){
  if(hornbill.connect(HOST_ADDRESS,CLIENT_ID)== 0)
    {
        Serial.println("Connected to AWS");
        delay(1000);

        if(0==hornbill.subscribe(TOPIC_NAME,mySubCallBackHandler))
        {
            Serial.println("Subscribe Successfull");
        }
        else
        {
            Serial.println("Subscribe Failed, Check the Thing Name and Certificates");
            while(1);
        }
    }
    else
    {
        Serial.println("AWS connection failed, Check the HOST Address");
        while(1);
    }
}

void setup()
{
  DEBUG_PORT.begin(38400);
  Serial.begin(38400);
  //SerialBT.setPin("1234");
  ELM_PORT.begin("ESP32test", false);
  connectOBD();
  //connectWifi();
  //connectAWS();
  Serial.println("Connected to ELM327");
  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  //SerialMon.println("Initializing modem...");
  //modem.restart();
}

bool requestData = true;
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
      Serial.println("sending to AWS...");
      requestData = false;
    }
  }
}

bool bufferEmpty = false;
bool queryReceive = false;
bool endQuery = false;
bool isCorrectResponse = false;
bool valueWritten = false;
void getDTC(){
      if (!valueWritten && bufferEmpty) {
    ELM_PORT.print(DTC);
    Serial.println("sending...");
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
        sendDTC();
        tick = 0;
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
}

void loop()
{
  if(msgReceived == 1)
    {
        msgReceived = 0;
        Serial.print("Received Message:");
        Serial.println(rcvdPayload);
    }
  if(requestData){
     getData();
  }else{
    if(tick > 60){
      getDTC();
    }
  }
  tick++;
  delay(1000);
}
