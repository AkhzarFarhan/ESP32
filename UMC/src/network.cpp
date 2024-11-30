#include "network.h"


Network::Network() : tempC(0.0), tempF(0.0), humidity(0.0)
{
    // Initialize member variables if needed
}

Network::~Network()
{
    // Clean up resources if any
}

void Network::fill(float tempC, float tempF, float humidity)
{
    this->tempC = tempC;
    this->tempF = tempF;
    this->humidity = humidity;
}

void Network::send()
{
    
}

void Network::receive()
{
    // Implementation of receive
}