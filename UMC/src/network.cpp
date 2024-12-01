#include "network.h"
#include <ArduinoJson.h>


Network::Network() : tempC(0.0), tempF(0.0), humidity(0.0)
{
    // Initialize member variables if needed
}

Network::~Network()
{
    // Clean up resources if any
}

void Network::set(float tempC, float tempF, float humidity)
{
    this->tempC = tempC;
    this->tempF = tempF;
    this->humidity = humidity;
}

void Network::reset()
{
    this->tempC = 0;
    this->tempF = 0;
    this->humidity = 0;
}

void Network::send()
{
    
}

void Network::receive()
{
    // Implementation of receive
}