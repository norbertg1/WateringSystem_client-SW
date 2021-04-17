#ifndef PERIPHERALS_HPP
#define PERIPHERALS_HPP

class flowmeter{
private:
    int flowmeter_int=0;
public:
    flowmeter();
    void flowmeter_callback();
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

#endif