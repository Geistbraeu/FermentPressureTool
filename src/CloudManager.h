#ifndef CLOUD_MANAGER_H
#define CLOUD_MANAGER_H

#include "cloud/BrewfatherProvider.h"
#include "cloud/CustomHTTPProvider.h"
#include "cloud/ThingSpeakProvider.h"

class CloudManager {
public:
	void init();
	void sendThingSpeak(const CloudPayload& payload);
	void sendBrewfather(const CloudPayload& payload);
	void sendCustomHttp(const CloudPayload& payload);
	unsigned long getLastTsSyncMs() const;
	unsigned long getLastBfSyncMs() const;
	unsigned long getLastHttpSyncMs() const;

private:
	ThingSpeakProvider thingSpeakProvider;
	BrewfatherProvider brewfatherProvider;
	CustomHTTPProvider customHttpProvider;
};

void initCloud();
void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp, uint32_t valveActivationsPerHour);
void sendDataToBrewfather(float voltage, float pressure, float temp, uint32_t valveActivationsPerHour);
void sendDataViaCustomHTTP(float voltage, float pressure, float pressureBar, float temp, uint32_t valveActivationsPerHour);
unsigned long cloudLastTsSyncMs();
unsigned long cloudLastBfSyncMs();
unsigned long cloudLastHttpSyncMs();

#endif // CLOUD_MANAGER_H
