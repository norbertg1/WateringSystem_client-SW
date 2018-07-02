# Home watering system and weather station

The hardware is based on ESP8266. Its powered from LiFe battery which is chargerd from solar panel. The hardware wakes up every 10 minute (default), and login to mosquito message broker (on ubuntu) and publishes temperature, moisture, flow, etc.. If it is needed it can turn on the irrigation with H-bridge.
The connection between MySQL and Mosquitto is handled with short program written in Python.

**The PCB contains:** EPS8266, LiFe charger, battery voltage sensor, BMP280 sensor, soil moisture sensor, H-bridge for ball valve motor, flow meter.

#### **MySQL structure and settings:**

See: https://github.com/xnorbi/Watering-system/blob/master/server_on_ubuntu/MySQL/database_initialization_commands

For photos see other folder.