#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <HTTPClient.h>

class Network
{
    private:
        float tempC;
        float tempF;
        float humidity;
    public:
        Network();
        ~Network();

        void set(float tempC, float tempF, float humidity);
        void reset();
        void send();
        void receive();
};

#endif // NETWORK_H