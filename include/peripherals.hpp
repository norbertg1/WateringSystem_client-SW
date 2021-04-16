#pragma once

//We are passing C++ member function into C callback

class flowmeter{
private:
    int flowmeter_int=0;
public:
    flowmeter();
    void flowmeter_interrupt();
    float get_volume();
    float get_velocity();
};

class voltage{
public:
    int voltage=0;
    void read_voltage();
};

class moisture{
public:
    float moisture=0;
    void read_moisture();
};

class thermometer{
public:
    float temperature;
    void read();
};

class humiditySensor{
public:
    float humidity=0;
    void read();
};

class pressureSensor{
public:
    float pressure=0;
    void read();
};