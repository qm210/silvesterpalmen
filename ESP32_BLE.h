#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

bool isConnected = false;
bool wasConnected = false;

class ServerCallbacks: public BLEServerCallbacks
{
  void onConnect(BLEServer *bleServer)
  {
    isConnected = true;
    Serial.println("is now connected!");
  }

  void onDisconnect(BLEServer *bleServer)
  {
    isConnected = false;
    Serial.println("is now disconnected!");
  }
};

class ChrctCallbacks: public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pChrct)
  {
    std::string rxValue = pChrct->getValue();

    for(std::size_t i = 0; i < received.size(); ++i)
    {
      if(received[i][0] == rxValue[0])
      {
        Serial.print("OVERWRITE RECEIVED ");
        Serial.println(received[i][0]);
        received[i] = rxValue;
        return;
      }
    }
    received.push_back(rxValue);
  }
};
