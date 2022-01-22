//BLE includes
#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLEScan.h"
#include "BLEAdvertisedDevice.h"
#include "BLEAddress.h"

//Buzzer includes
#include "melodies.h"
#include "notes.h"

//Variables for scan and connections
static boolean connection =  false;
static boolean isConnected = false;
static boolean search = false;
bool connected = false;

//Beacon adresses:
String BLE_Meta = "eb:76:bc:f0:c2:6f"; //Bluetoothadresse des ersten Tags
String BLE_Paw_b = "ff:ff:22:30:15:65";
String BLE_Giga = "58:9e:c6:00:df:da";
String BLE_Beacon = "";

//variables for getting distance
static int recieved_rssi = 0;
static BLEAddress *pServerAddress;
BLEScan* pBLEScan ;
int scanTime = 30; //Seconds
static BLEClient*  pClient;
static BLEAdvertisedDevice* myDevice;
static BLERemoteCharacteristic* pRemoteCharacteristic;
volatile float txPower = -65;
float ratio = 0;
float distance_meter = 0;
int enviorment_factor = 4;

//Buzzer variable
const int BUZZER_PIN = 4; //PIN Nummer des Buzzers
int channel = 0;
int tempo = 2.5;
int freq = 611;
int resolution = 8;
int dutyCycle = 125;
int begin_num = 0;
int search_num = 0;
int found_num = 0;
boolean buzz = false;

//Buttons
const int BUTTON_Meta = 18;
const int BUTTON_Giga = 23;
const int BUTTON_Paw_b = 22;

//Buttonstates
volatile int BUTTON_state_Meta = 1;
volatile int BUTTON_state_Giga = 1;
volatile int BUTTON_state_Paw_b = 1;
volatile int BUTTON_state_Paw_p = 1;
int counter = 1;


// Function for tones with PWM
void beep(int channel, int note, int duration) {
  ledcWriteTone(channel, note);
  delay(duration);
  ledcWriteTone(channel, 0);
}

//Melodies
void searching() {
  search_num = sizeof(search_tone) / sizeof(int);
  for (int i = 0; i < search_num; i++) {
    beep(channel, search_tone[i], search_duration[i] / tempo);
  }
  ledcWrite(channel, 0);
}

void found() {
  found_num = sizeof(found_tone) / sizeof(int);
  for (int i = 0; i < found_num; i++) {
    beep(channel, found_tone[i], found_duration[i] / tempo);
  }
  ledcWrite(channel, 0);
}

void hello() {
  begin_num = sizeof(begin_tones) / sizeof(int);
  for (int i = 0; i < begin_num; i++)
  {
    beep(channel, begin_tones[i], begin_duration[i] / tempo);
    delay(begin_duration[i] / tempo);
  }
  ledcWrite(channel, 0);
}

//Callback from BLE Devices
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice) //Wenn Beacon gefunden wurde
    {
      Serial.print(advertisedDevice.getAddress().toString().c_str()); //Adresse des gefundenen Device ausgeben
      Serial.println(advertisedDevice.toString().c_str());
      if (advertisedDevice.getAddress().equals(*pServerAddress))
      {
        Serial.print("Searched Device Found!\n");
        BLEDevice::getScan()->stop();                               // Scanvorgang beenden
        myDevice = new BLEAdvertisedDevice(advertisedDevice);
        connection = true;
        search = false;
      }
      Serial.println("");
    }
};


// Setting connection variables
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
      Serial.println("Verbunden");
      isConnected = true;
      connection = false;
      search = false;
    }

    void onDisconnect(BLEClient* pclient) {
      isConnected = false;
      connection = false;
      search = true;
      Serial.println("Verbindung verloren");
    }
};

//Button-Interrupt for changing search device
void pin_ISR() {

  BUTTON_state_Meta = digitalRead(BUTTON_Meta);
  BUTTON_state_Giga = digitalRead(BUTTON_Giga);
  BUTTON_state_Paw_b = digitalRead(BUTTON_Paw_b);

  if (!BUTTON_state_Meta) {
    Serial.println("Meta!");
    BLEDevice::getScan()->stop();
    BLE_Beacon = BLE_Meta;
    pServerAddress = new BLEAddress(BLE_Beacon.c_str());
    txPower = -61;
    search = true;
    buzz = false;
  } else if (!BUTTON_state_Giga) {
    Serial.println("Giga!");
    BLEDevice::getScan()->stop();
    BLE_Beacon = BLE_Giga;
    pServerAddress = new BLEAddress(BLE_Beacon.c_str());
    txPower = -66;
    search = true;
    buzz = false;
  } else if (!BUTTON_state_Paw_b) {
    Serial.println("Blue!");
    BLEDevice::getScan()->stop();
    BLE_Beacon = BLE_Paw_b;
    pServerAddress = new BLEAddress(BLE_Beacon.c_str());
    txPower = -81;
    search = true;
    buzz = false;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("");

  //Input buttons
  pinMode(BUTTON_Meta, INPUT_PULLUP);
  pinMode(BUTTON_Giga, INPUT_PULLUP);
  pinMode(BUTTON_Paw_b, INPUT_PULLUP);

  //Interrrupt when button is released
  attachInterrupt(BUTTON_Meta, pin_ISR, FALLING);
  attachInterrupt(BUTTON_Giga, pin_ISR, FALLING);
  attachInterrupt(BUTTON_Paw_b, pin_ISR, FALLING);

  Serial.println("Starte BLE Scanner!");

  //BLE initialization
  BLEDevice::init("");
  pBLEScan  = BLEDevice::getScan(); //create new scan
  pBLEScan  -> setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan  -> setActiveScan(true); //faster results
  pBLEScan -> setInterval(1000);
  pBLEScan->setWindow(1000);


  //Buzzer initialization
  pinMode(BUZZER_PIN, OUTPUT);       // set ESP32 pin to output mode
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(BUZZER_PIN, channel);

  //Welcome routine
  int dutyCycle = 125;
  ledcWrite(channel, dutyCycle);
  delay(500);
  hello();
}

void loop() {
  delay(100);

  //while searching
  if (search)  {
    searching();
    pBLEScan->start(scanTime);
    
    delay(100);
  }

  //device found
  if (connection)
  {
    Serial.print("Verbinde mit ");
    Serial.println(myDevice->getAddress().toString().c_str());
    pClient = BLEDevice::createClient();
    pClient->disconnect();
    pClient->setClientCallbacks(new MyClientCallback());
    pClient->connect(myDevice);
    buzz = true;
    found();
  }


  if (isConnected) // Paired with searched device
  {
    delay(200);

    while (buzz)
    {
      for (int i = 0; i < 5; i++) {
        recieved_rssi = recieved_rssi + (int)pClient->getRssi();
      }

      recieved_rssi = (int) recieved_rssi / 5;
      Serial.println(recieved_rssi);
      ratio = (txPower - recieved_rssi) / (10 * enviorment_factor);
      distance_meter  = round( pow(10, ratio));
      recieved_rssi = 0;
      Serial.printf("Distance: %f \n", distance_meter );
      delay(2000);
      //Buzzer
      ledcWrite(channel, dutyCycle);
      delay(200);

      if (distance_meter < 30) { //cut off extremes
        for (int i = 0; i < distance_meter; i++) {
          for (int i = 0; i < 2; i++)
          {
            beep(channel, meter_tone[i], meter_tone[i] / tempo);
            delay(meter_duration[i] / tempo);
          } if (!buzz) {
            break;
          }
        }
      }
      ledcWrite(channel, 0);
    }
  }
}
