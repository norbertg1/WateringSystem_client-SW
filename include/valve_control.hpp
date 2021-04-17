#ifndef VALVE_CONTROL_HPP
#define VALVE_CONTROL_HPP

 

class valve_control{
public:
    int locsolo_state = 0, on_off_command = 0, winter_state = 0, valve_timeout = 0;
    float moisture=0;
    void turn_on();
    void turn_off();
    int state();
    void is_open();
    void is_closed();
    void winter_mode();
    void test();
    void open_on_switch();
};

#endif