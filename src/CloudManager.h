#ifndef CLOUD_MANAGER_H
#define CLOUD_MANAGER_H

void initCloud();
void sendDataToThingSpeak(float voltage, float pressure, float pressureBar, float temp);
void sendDataToBrewfather(float voltage, float pressure, float temp);

#endif // CLOUD_MANAGER_H
