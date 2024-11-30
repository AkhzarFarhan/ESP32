#ifndef NETWORK_H
#define NETWORK_H


class Network
{
    private:
        float tempC;
        float tempF;
        float humidity;
    public:
        Network();
        ~Network();

        void fill(float tempC, float tempF, float humidity);
        void send();
        void receive();
};

#endif // NETWORK_H