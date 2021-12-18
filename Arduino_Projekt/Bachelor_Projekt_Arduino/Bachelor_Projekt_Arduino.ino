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
static boolean connected_ = false;
static boolean search = false;
bool connected = false;


//Beacon adresses:
String BLE_Meta = "eb:76:bc:f0:c2:6f"; //Bluetoothadresse des ersten Tags
String BLE_Paw_b = "ff:ff:22:30:15:65";
String BLE_Paw_p = "ff:ff:cc:0b:93:le";
String BLE_Giga = "58:9e:c6:00:df:da";
String BLE_Beacon = "";


//BLE variable
static int recieved_rssi = 0;

//Buzzer variable
const int BUZZER_PIN = 4; //PIN Nummer des Buzzers
int channel = 0;
int tempo = 2.5;
int freq = 611;
int resolution = 8;
int begin_num = 0;
int dutyCycle = 125;

//Buttons
const int BUTTON_Meta = 18;
const int BUTTON_Giga = 23;
const int BUTTON_Paw_b = 19;
const int BUTTON_Paw_p = 22;

//Buttonstates
volatile int BUTTON_state_Meta = 1;
volatile int BUTTON_state_Giga = 1;
volatile int BUTTON_state_Paw_b = 1;
volatile int BUTTON_state_Paw_p = 1;

int counter = 1;

static BLEAddress *pServerAddress;
BLEScan* pBLEScan ;
int scanTime = 30; //Sekunden

static BLEClient*  pClient;
static BLEAdvertisedDevice* myDevice;
static BLERemoteCharacteristic* pRemoteCharacteristic;

void beep(int channel, int note, int duration) {
  ledcWriteTone(channel, note);
  delay(duration);
  ledcWriteTone(channel, 0);
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice) //Wenn Beacon gefunden wurde
    {
      Serial.print(advertisedDevice.getAddress().toString().c_str()); //Adresse des gefundenen Device ausgeben
      // Serial.println(advertisedDevice.toString().c_str());
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

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
      Serial.println("Verbunden");
      connected_ = true;
      connection = false;
    }

    void onDisconnect(BLEClient* pclient) {
      connected_ = false;
      connection = false;
      search = true;
      Serial.println("Verbindung verloren");
    }
};

//Button-Interrupt
void pin_ISR() {

  BUTTON_state_Meta = digitalRead(BUTTON_Meta);
  BUTTON_state_Giga = digitalRead(BUTTON_Giga);
  BUTTON_state_Paw_b = digitalRead(BUTTON_Paw_b);
  BUTTON_state_Paw_p = digitalRead(BUTTON_Paw_p);

  if (!BUTTON_state_Meta) {
    Serial.println("Meta!");
    BLEDevice::getScan()->stop();
    delay(500);
    BLE_Beacon = BLE_Meta;
    pServerAddress = new BLEAddress(BLE_Beacon.c_str());
    search = true;
  } else if (!BUTTON_state_Giga) {
    Serial.println("Giga!");
    BLEDevice::getScan()->stop();
    delay(500);
    BLE_Beacon = BLE_Giga;
    pServerAddress = new BLEAddress(BLE_Beacon.c_str());
    search = true;
  } else if (!BUTTON_state_Paw_b) {
    Serial.println("Blue!");
    BLEDevice::getScan()->stop();
    delay(500);
    BLE_Beacon = BLE_Paw_b;
    pServerAddress = new BLEAddress(BLE_Beacon.c_str());
    search = true;
  } else if (!BUTTON_state_Paw_p) {
    Serial.println("Pink!");
    BLEDevice::getScan()->stop();
    delay(500);
    BLE_Beacon = BLE_Paw_p;
    pServerAddress = new BLEAddress(BLE_Beacon.c_str());
    search = true;
  }

}


void setup() {
  Serial.begin(115200);
  Serial.println("");

  //Button
  pinMode(BUTTON_Meta, INPUT_PULLUP);
  pinMode(BUTTON_Giga, INPUT_PULLUP);
  pinMode(BUTTON_Paw_b, INPUT_PULLUP);
  pinMode(BUTTON_Paw_p, INPUT_PULLUP);

  attachInterrupt(BUTTON_Meta, pin_ISR, CHANGE);
  attachInterrupt(BUTTON_Giga, pin_ISR, CHANGE);
  attachInterrupt(BUTTON_Paw_b, pin_ISR, CHANGE);
  attachInterrupt(BUTTON_Paw_p, pin_ISR, CHANGE);

  Serial.println("Starte BLE Scanner!");

  //BLE
  BLEDevice::init("");
  pBLEScan  = BLEDevice::getScan(); //create new scan
  pBLEScan  -> setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan  -> setActiveScan(true); //faster results
  pBLEScan -> setInterval(1000);
  pBLEScan->setWindow(1000);


  //Buzzer
  pinMode(BUZZER_PIN, OUTPUT);       // set ESP32 pin to output mode
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(BUZZER_PIN, channel);

  //Welcome
  int dutyCycle = 125;
  Serial.println("Hello!\n");
  ledcWrite(channel, dutyCycle);
  delay(1000);
  begin_num = sizeof(begin_tones) / sizeof(int);

  for (int i = 0; i < begin_num; i++)
  {
    beep(channel, begin_tones[i], begin_duration[i] / tempo);
    delay(begin_duration[i] / tempo);
  }
  ledcWrite(channel, 0);
}

void loop() {
  delay(100);

  if (search)  {
    pBLEScan->start(scanTime);
    delay(100);
  }

  if (connection == true)
  {
    Serial.print("Verbinde mit ");
    Serial.println(myDevice->getAddress().toString().c_str());
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    pClient->connect(myDevice);
  }

  if (connected_) // Paired
  {
    delay(500);
    float txPower = -65;

    if (!counter)
    {
      for (int i = 0; i < 3; i++) {
        recieved_rssi = recieved_rssi + (int)pClient->getRssi();
      }


      recieved_rssi = (int) recieved_rssi / 3;
      Serial.println(recieved_rssi);
      float ratio = (txPower - recieved_rssi) / (10 * 2);
      float distance_meter  = pow(10, ratio);
      recieved_rssi = 0;
      // Serial.printf("Rssi: %d \n", recieved_rssi);
      Serial.printf("Distance: %f \n", distance_meter );

      //Buzzer
      ledcWrite(channel, dutyCycle);
      delay(500);

      for (int i = 0; i < distance_meter; i++) {
        for (int i = 0; i < 2; i++)
        {
          beep(channel, meter_tone[i], meter_tone[i] / tempo);
          delay(meter_duration[i] / tempo);
        }
      //  Serial.println("Test");
      }

      ledcWrite(channel, 0);
     // Serial.println("test2!\n");
      counter = 5;
    }
    else {
      counter--;
    }
  }
}
