#include "CloudManager.h"

static CloudManager cloudManager;

void CloudManager::init() {
  thingSpeakProvider.init();
  brewfatherProvider.init();
  customHttpProvider.init();
}

void CloudManager::sendThingSpeak(const CloudPayload& payload) {
  thingSpeakProvider.send(payload);
}

void CloudManager::sendBrewfather(const CloudPayload& payload) {
  brewfatherProvider.send(payload);
}

void CloudManager::sendCustomHttp(const CloudPayload& payload) {
  customHttpProvider.send(payload);
}

unsigned long CloudManager::getLastTsSyncMs() const  { return thingSpeakProvider.getLastSendMs(); }
unsigned long CloudManager::getLastBfSyncMs() const   { return brewfatherProvider.getLastSendMs(); }
unsigned long CloudManager::getLastHttpSyncMs() const { return customHttpProvider.getLastSendMs(); }

unsigned long cloudLastTsSyncMs()   { return cloudManager.getLastTsSyncMs(); }
unsigned long cloudLastBfSyncMs()   { return cloudManager.getLastBfSyncMs(); }
unsigned long cloudLastHttpSyncMs() { return cloudManager.getLastHttpSyncMs(); }

void initCloud() {
  cloudManager.init();
}

void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp, uint32_t valveActivationsPerHour) {
  CloudPayload payload;
  payload.voltage = voltage;
  payload.pressurePsi = pressure;
  payload.pressureBar = pressureBar;
  payload.temperatureC = temp;
  payload.valveActivationsPerHour = valveActivationsPerHour;
  cloudManager.sendThingSpeak(payload);
}

void sendDataToBrewfather(float voltage, float pressure, float temp, uint32_t valveActivationsPerHour) {
  CloudPayload payload;
  payload.voltage = voltage;
  payload.pressurePsi = pressure;
  payload.pressureBar = pressure * 0.0689476f;
  payload.temperatureC = temp;
  payload.valveActivationsPerHour = valveActivationsPerHour;
  cloudManager.sendBrewfather(payload);
}

void sendDataViaCustomHTTP(float voltage, float pressure, float pressureBar, float temp, uint32_t valveActivationsPerHour) {
  CloudPayload payload;
  payload.voltage = voltage;
  payload.pressurePsi = pressure;
  payload.pressureBar = pressureBar;
  payload.temperatureC = temp;
  payload.valveActivationsPerHour = valveActivationsPerHour;
  cloudManager.sendCustomHttp(payload);
}
