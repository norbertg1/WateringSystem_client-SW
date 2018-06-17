# -*- coding: utf-8 -*-
#!/usr/bin/env python


import sys; sys.path.append('/home/odroid/.local/lib/python2.7/site-packages')
import MySQLdb
import paho.mqtt.client as mqtt
import datetime
import time
from sqlalchemy import *
from sqlalchemy.orm import *
import ssl
ssl.match_hostname = lambda cert, hostname: True
from tendo import singleton
import threading 
import pyowm

print "MQTT_MySQL handler starting.",
me = singleton.SingleInstance() # will sys.exit(-1) if other instance is running

#Ha ez a sor ki van kommentezve akkor ez a process amit a crontabe 15 precenként elindít kiütök egymást és ha pont megy a locsoló le fog állni
#ellenben így lehet manuálisan elindítani ezt a kódot módosítás után mert megszünteti az előző futását

engine = create_engine("mysql+mysqldb://root:1234@localhost/watering_server?host=localhost?port=3306")
conn = engine.connect()
metadata = MetaData(engine)
Session = sessionmaker(bind=engine)
session = Session()

users_table = Table('users', metadata, autoload=True);
devices_table = Table('devices', metadata, autoload=True);
data_table = Table('data', metadata, autoload=True);
scheduled_irrigation_table = Table('scheduled_irrigation', metadata, autoload=True);
scheduled_irrigation_one_time_table = Table('scheduled_irrigation_one_time', metadata, autoload=True);
hottest_days_irrigation_table = Table('hottest_days_irrigation', metadata, autoload=True);
pairs_table = Table('pairs', metadata, autoload=True);

owm = pyowm.OWM('80c4722573ef8abe4b03228d9465fe09') 

data = {}

class database_data:
    data_table = Table('data', metadata, autoload=True);

    def __init__( self, DEVICE_NAME=0, DEVICE_ID=0, TEMPERATURE=0, HUMIDITY=0, MOISTURE=0, PRESSURE=0, VERSION=0, RST_REASON=0, WATER_VOLUME=0, WATER_VOLUME_X=0, WATER_VELOCITY=0, MM=0, VOLTAGE=0, ON_OFF_STATE=0, TEMPERATURE_POINTS=0, AWAKE_TIME=0, AWAKE_TIME_X=0, RSSI=0,
                  on_time=datetime.datetime.now(), off_time=datetime.datetime.now()):

        self.DEVICE_NAME = DEVICE_NAME
        self.DEVICE_ID = DEVICE_ID
        self.TEMPERATURE = TEMPERATURE
        self.HUMIDITY = HUMIDITY
        self.MOISTURE = MOISTURE
        self.PRESSURE = PRESSURE
        self.VERSION = VERSION
        self.RST_REASON = RST_REASON
        self.WATER_VOLUME = WATER_VOLUME
        self.WATER_VOLUME_X = WATER_VOLUME_X
        self.WATER_VELOCITY = WATER_VELOCITY
        self.MM = MM
        self.VOLTAGE = VOLTAGE
        self.ON_OFF_STATE = ON_OFF_STATE
        self.TEMPERATURE_POINTS = TEMPERATURE_POINTS
        self.on_time = on_time
        self.off_time = off_time
        self.AWAKE_TIME = AWAKE_TIME
        self.AWAKE_TIME_X = AWAKE_TIME_X
        self.RSSI = RSSI


    def save_database_data(self):
        trans = conn.begin()
        database_data.data_table.insert().execute(DEVICE_ID=self.DEVICE_ID, DEVICE_NAME = self.DEVICE_NAME, LAST_LOGIN=time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), 
                                                    TEMPERATURE=self.TEMPERATURE, HUMIDITY=self.HUMIDITY, MOISTURE=self.MOISTURE, PRESSURE=self.PRESSURE, VERSION=self.VERSION,
                                                    RST_REASON=self.RST_REASON, WATER_VOLUME=self.WATER_VOLUME, WATER_VELOCITY=self.WATER_VELOCITY, MM=self.MM, VOLTAGE=self.VOLTAGE,
                                                    ON_OFF_STATE=self.ON_OFF_STATE, TEMPERATURE_POINTS = self.TEMPERATURE_POINTS, RSSI=self.RSSI, AWAKE_TIME=self.AWAKE_TIME)
        trans.commit()

def on_connect(client, userdata, rc, m):
    client.subscribe("+/DEVICE_NAME")
    client.subscribe("+/TEMPERATURE")
    client.subscribe("+/HUMIDITY")
    client.subscribe("+/MOISTURE")
    client.subscribe("+/PRESSURE")
    client.subscribe("+/VERSION")
    client.subscribe("+/RST_REASON")
    client.subscribe("+/VOLTAGE")
    client.subscribe("+/READY_FOR_DATA")
    client.subscribe("+/ON_OFF_STATE")
    client.subscribe("+/FLOWMETER_VOLUME")
    client.subscribe("+/FLOWMETER_VOLUME_X")
    client.subscribe("+/FLOWMETER_VELOCITY")
    client.subscribe("+/AWAKE_TIME")
    client.subscribe("+/AWAKE_TIME_X")
    client.subscribe("+/RSSI")
    client.subscribe("+/DEBUG")
    client.subscribe("+/END")

def on_message(client, userdata, msg):
    device_id=msg.topic[0:(msg.topic).find("/")]
    variable_type=msg.topic[(msg.topic).find("/")+1:len(msg.topic)]
    handle_database(device_id, variable_type, msg.payload)
 
def handle_database(device_id, variable_type, value):
    if (data.has_key(device_id) == 0) and session.query(exists().where(devices_table.c.DEVICE_ID==device_id)).scalar():
        print "device_id logged in:", device_id
        data[device_id] = database_data()
        data[device_id].DEVICE_ID = device_id
    if(data.has_key(device_id)):
        print (variable_type + ':' + value)
        if(variable_type == 'DEVICE_NAME'):         data[device_id].DEVICE_NAME = value
        if(variable_type == 'TEMPERATURE'):         data[device_id].TEMPERATURE = value
        if(variable_type == 'HUMIDITY'):            data[device_id].HUMIDITY = value
        if(variable_type == 'MOISTURE'):            data[device_id].MOISTURE = value
        if(variable_type == 'PRESSURE'):            data[device_id].PRESSURE = value
        if(variable_type == 'VERSION'):             data[device_id].VERSION = value
        if(variable_type == 'RST_REASON'):          data[device_id].RST_REASON = value
        if(variable_type == 'FLOWMETER_VELOCITY'):  data[device_id].WATER_VELOCITY = value
        if(variable_type == 'VOLTAGE'):             data[device_id].VOLTAGE = value
        if(variable_type == 'RSSI'):                data[device_id].RSSI = value
        if(variable_type == 'AWAKE_TIME'):          data[device_id].AWAKE_TIME = value
        if(variable_type == 'AWAKE_TIME_X'):        data[device_id].AWAKE_TIME_X = value
        if(variable_type == 'READY_FOR_DATA'):      send_message_to_device(device_id, data)
        if(variable_type == 'FLOWMETER_VOLUME_X'):  data[device_id].WATER_VOLUME_X = value
        if(variable_type == 'FLOWMETER_VOLUME'):    
            data[device_id].WATER_VOLUME = value
            column=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
            if(column['AREA'] != 0 ):   data[device_id].MM = float(value)/column['AREA']
        if(variable_type == 'ON_OFF_STATE'):                #ha be volt kapcsolva a locsolás (amit az adatbázisból olvasok ki) és most olyan érték jön,
            collumn = data_table.select(data_table.c.DEVICE_ID == device_id).order_by(desc('LAST_LOGIN')).execute().fetchone()
            if collumn is not None:
                if collumn['ON_OFF_STATE'] == 1 and int(value) == 0:   #hogy pont most kapcsolt ki, akkor elmentem a kikapcsolási időpontot az adatbázisba
                    conn.execute("UPDATE devices SET LAST_ON_TIME = \'"+ time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()) + "\' where DEVICE_ID = \'" + device_id + "\'")
            data[device_id].ON_OFF_STATE = value         
        if(variable_type == 'END'):                     
            print "Local time:" + time.strftime("%Y-%b-%d %H:%M:%S", time.localtime()) + "\n-------------------------------------------------------------"
            data[device_id].save_database_data()
            if not data[device_id].on_time < datetime.datetime.now() < data[device_id].off_time:
                del data[device_id];   #csak akkor ha nem megy előre beprogramozott öntözés

def send_message_to_device(device_id, data): #send on_off command
    print "database read time", datetime.datetime.now().time()
    on_off=read_command_from_database(device_id, data)
    print datetime.datetime.now().time()
    client.publish(device_id + "/ON_OFF_COMMAND", on_off)
    print 'Command for the device', device_id, ':', on_off
    set_delay_sleep_times(device_id)

def set_delay_sleep_times(device_id):
    collumn=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    #print collumn['SLEEP_TIME']
    client.publish(device_id + "/SLEEP_TIME", str(collumn['SLEEP_TIME']))
    client.publish(device_id + "/DELAY_TIME", str(collumn['DELAY_TIME']))
    client.publish(device_id + "/REMOTE_UPDATE", str(collumn['REMOTE_UPDATE']))
    
def read_command_from_database(device_id, data): 
    collumn=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    if(collumn['ON_COMMAND']): return 1
    #if(scheduled_irrigation(device_id)): return 1
    #if(scheduled_irrigation_one_time(device_id)): return 1
    if(scheduled_irrigation_lenght(device_id, data)): return 1
    if(scheduled_irrigation_liters(device_id, data)): return 1
    if(scheduled_irrigation_one_time_lenght(device_id, data)): return 1
    if(scheduled_irrigation_one_time_liters(device_id, data)): return 1
    #folytatni ha ez a funkcio 1est ad vissza az bekapcsolja az adott eszközön az öntözést
    return 0

def scheduled_irrigation(device_id, data):
    collumn=scheduled_irrigation_table.select(scheduled_irrigation_table.c.DEVICE_ID == device_id).execute()
    for row in collumn:
        if 0 < (datetime.datetime.now()-datetime.datetime.combine(datetime.date.today(), row['ON_TIME'])).total_seconds()/60 < 100 and not row['DONE_FOR_TODAY']:
            conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row['ON_TIME']) + "\'")
            data[device_id].on_time = datetime.datetime.now()
            on_time_lenght = row['ON_TIME_LENGHT']
            data[device_id].off_time = (datetime.datetime.combine(datetime.date.today(), data[device_id].on_time.time()) + datetime.timedelta(minutes=on_time_lenght))
            break
    if data[device_id].on_time < datetime.datetime.now() < data[device_id].off_time:    return 1
    return 0

def scheduled_irrigation_one_time(device_id, data):   #Regi funkció, itt ha locsolás közben megszakadt a kapcsolat nem folytatódott a locsolás
    collumn=scheduled_irrigation_one_time_table.select(scheduled_irrigation_one_time_table.c.DEVICE_ID == device_id).execute()
    row_devices=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()   #ez ahhoz hogy esőben ne öntözzön
    for row in collumn:
        if 0 < (datetime.datetime.now()-datetime.datetime.combine(datetime.date.today(), row['ON_TIME'])).total_seconds()/60 < 100 and not row['DONE_FOR_TODAY']:
            conn.execute("UPDATE scheduled_irrigation_one_time SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row['ON_TIME']) + "\'")
            weather_code=get_wheather_code(row_devices['LATID'], row_devices['LONGIT'])
            if (int(weather_code/100) == 2 or int(weather_code/100) == 5):
                print ("Eso vagy vihar van, ne legyen ontozes")
                print "weather_code:", datetime.datetime.now(), weather_code
                return 0                
            data[device_id].on_time = datetime.datetime.now()
            on_time_lenght = row['ON_TIME_LENGHT']
            data[device_id].off_time = (datetime.datetime.combine(datetime.date.today(), data[device_id].on_time.time()) + datetime.timedelta(minutes=on_time_lenght))
            break
    if data[device_id].on_time < datetime.datetime.now() < data[device_id].off_time:    return 1
    return 0

def scheduled_irrigation_lenght(device_id, data_device):     #Regi funkció, itt ha locsolás közben megszakadt a kapcsolat nem folytatódott a locsolás
    column_scheduled=scheduled_irrigation_table.select(scheduled_irrigation_table.c.DEVICE_ID == device_id).execute()
    column_data=data_table.select().where((data_table.c.DEVICE_ID == device_id) & (data_table.c.LAST_LOGIN > datetime.datetime.now() - datetime.timedelta(hours=3))).execute()
    row_devices=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    weather_code=get_wheather_code(row_devices['LATID'], row_devices['LONGIT'])
    print "lenght weather_code:", datetime.datetime.now(), weather_code
    data = {}; i=0; total_irrigation_time=0
    for row_data in column_data:
        data[i] = {}
        data[i]["ON_OFF_STATE"] = row_data["ON_OFF_STATE"]
        data[i]["AWAKE_TIME"] = row_data["AWAKE_TIME"]
        i+=1
    for i in range(len(data)):
        if i >= 1 and i < len(data)-1 and (data[i]["ON_OFF_STATE"] == 1 or data[i-1]["ON_OFF_STATE"] == 1) and data[i+1]["AWAKE_TIME"]<data[i]["AWAKE_TIME"]>data[i-1]["AWAKE_TIME"] and data[i]["AWAKE_TIME"] > 30:
            total_irrigation_time  += float(data[i]["AWAKE_TIME"])
            print "lenght", data[i]["AWAKE_TIME"], total_irrigation_time
        if i == len(data)-1 and data[i]["ON_OFF_STATE"] == 1:
            total_irrigation_time  += float(data[i]["AWAKE_TIME"])
            print "lenght lendata", data[i]["AWAKE_TIME"], total_irrigation_time
    total_irrigation_time += float(data_device[device_id].AWAKE_TIME_X)
    print "data_device[device_id].AWAKE_TIME_X", data_device[device_id].AWAKE_TIME_X
    for row_scheduled in column_scheduled:
        if datetime.datetime.now().time() > row_scheduled['ON_TIME'] < (datetime.datetime.now() + datetime.timedelta(hours=3)).time() and (total_irrigation_time-10) < row_scheduled['ON_TIME_LENGHT']*60 and not row_scheduled['DONE_FOR_TODAY'] and not row_scheduled['ON_TIME_LITERS'] and not row_scheduled['ON_TIME_MM']:
            if (int(weather_code/100) == 2 or int(weather_code/100) == 5):
                print ("Eso vagy vihar van, ne legyen ontozes")
                print "weather_code:", datetime.datetime.now(), weather_code
                conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row_scheduled['ON_TIME']) + "\'")
            else: return 1
        if datetime.datetime.now().time() > row_scheduled['ON_TIME'] < (datetime.datetime.now() + datetime.timedelta(hours=3)).time() and (total_irrigation_time-10) > row_scheduled['ON_TIME_LENGHT']*60 and not row_scheduled['DONE_FOR_TODAY'] and not row_scheduled['ON_TIME_LITERS'] and not row_scheduled['ON_TIME_MM']:
            conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row_scheduled['ON_TIME']) + "\'")
    return 0

def scheduled_irrigation_one_time_lenght(device_id, data_device):
    column_scheduled=scheduled_irrigation_one_time_table.select(scheduled_irrigation_one_time_table.c.DEVICE_ID == device_id).execute()
    column_data=data_table.select().where((data_table.c.DEVICE_ID == device_id) & (data_table.c.LAST_LOGIN > datetime.datetime.now() - datetime.timedelta(hours=3))).execute()
    row_devices=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    weather_code=get_wheather_code(row_devices['LATID'], row_devices['LONGIT'])
    print "one_time_lenght weather_code:", datetime.datetime.now(), weather_code
    data = {}; i=0; total_irrigation_time=0
    for row_data in column_data:
        data[i] = {}
        data[i]["ON_OFF_STATE"] = row_data["ON_OFF_STATE"]
        data[i]["AWAKE_TIME"] = row_data["AWAKE_TIME"]
        i+=1
    for i in range(len(data)):
        if i >= 1 and i < len(data)-1 and (data[i]["ON_OFF_STATE"] == 1 or data[i-1]["ON_OFF_STATE"] == 1) and data[i+1]["AWAKE_TIME"]<data[i]["AWAKE_TIME"]>data[i-1]["AWAKE_TIME"] and data[i]["AWAKE_TIME"] > 30:
            total_irrigation_time  += float(data[i]["AWAKE_TIME"])
            print "lenght", data[i]["AWAKE_TIME"], total_irrigation_time
        if i == len(data)-1 and data[i]["ON_OFF_STATE"] == 1:
            total_irrigation_time  += float(data[i]["AWAKE_TIME"])
            print "lenght end", data[i]["AWAKE_TIME"], total_irrigation_time
    total_irrigation_time += float(data_device[device_id].AWAKE_TIME_X)
    for row_scheduled in column_scheduled:
        if datetime.datetime.now().time() > row_scheduled['ON_TIME'] < (datetime.datetime.now() + datetime.timedelta(hours=3)).time() and (total_irrigation_time-10) < row_scheduled['ON_TIME_LENGHT']*60 and not row_scheduled['DONE_FOR_TODAY'] and not row_scheduled['ON_TIME_LITERS'] and not row_scheduled['ON_TIME_MM']:
            if (int(weather_code/100) == 2 or int(weather_code/100) == 5):
                print ("Eso vagy vihar van, ne legyen ontozes")
                print "weather_code:", datetime.datetime.now(), weather_code
                conn.execute("UPDATE scheduled_irrigation_one_time SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row_scheduled['ON_TIME']) + "\'")
            else: return 1
        if datetime.datetime.now().time() > row_scheduled['ON_TIME'] < (datetime.datetime.now() + datetime.timedelta(hours=3)).time() and (total_irrigation_time-10) > row_scheduled['ON_TIME_LENGHT']*60 and not row_scheduled['DONE_FOR_TODAY'] and not row_scheduled['ON_TIME_LITERS'] and not row_scheduled['ON_TIME_MM']:
            conn.execute("UPDATE scheduled_irrigation_one_time SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row_scheduled['ON_TIME']) + "\'")
    return 0

def scheduled_irrigation_liters(device_id, data_device):
    column_scheduled=scheduled_irrigation_table.select(scheduled_irrigation_table.c.DEVICE_ID == device_id).execute()
    column_data=data_table.select().where((data_table.c.DEVICE_ID == device_id) & (data_table.c.LAST_LOGIN > datetime.datetime.now() - datetime.timedelta(hours=3))).execute()
    row_devices=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    weather_code=get_wheather_code(row_devices['LATID'], row_devices['LONGIT'])
    print "one_time_liters weather_code:", datetime.datetime.now(), weather_code
    data = {}; i=0; total_irrigation_volume=0
    for row_data in column_data:
        data[i] = {}
        data[i]["ON_OFF_STATE"] = row_data["ON_OFF_STATE"]
        data[i]["WATER_VOLUME"] = row_data["WATER_VOLUME"]
        i+=1      
    for i in range(len(data)):
        if i >= 1 and i < len(data)-1 and (data[i]["ON_OFF_STATE"] == 1 or data[i-1]["ON_OFF_STATE"] == 1) and data[i+1]["WATER_VOLUME"]<data[i]["WATER_VOLUME"]>data[i-1]["WATER_VOLUME"] and data[i]["WATER_VOLUME"] > 0:
            total_irrigation_volume  += float(data[i]["WATER_VOLUME"])
            print "volume", data[i]["WATER_VOLUME"], total_irrigation_volume
        if i == len(data)-1 and data[i]["ON_OFF_STATE"] == 1:
            total_irrigation_volume  += float(data[i]["WATER_VOLUME"])
            print "volume end", data[i]["WATER_VOLUME"], total_irrigation_volume
    total_irrigation_volume  += float(data_device[device_id].WATER_VOLUME_X)
    print "data_device[device_id].WATER_VOLUME_X", data_device[device_id].WATER_VOLUME_X
    for row_scheduled in column_scheduled:
        if row_scheduled['ON_TIME_MM'] > 0:
            liters = row_scheduled['ON_TIME_MM']*row_devices["AREA"]
        else:
            liters = row_scheduled['ON_TIME_LITERS']
        if datetime.datetime.now().time() > row_scheduled['ON_TIME'] < (datetime.datetime.now() + datetime.timedelta(hours=3)).time() and total_irrigation_volume < liters and not row_scheduled['DONE_FOR_TODAY'] and not row_scheduled['ON_TIME_LENGHT']:
            if (int(weather_code/100) == 2 or int(weather_code/100) == 5):
                print ("Eso vagy vihar van, ne legyen ontozes")
                print "weather_code:", datetime.datetime.now(), weather_code
                conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row_scheduled['ON_TIME']) + "\'")
            else: return 1
        if datetime.datetime.now().time() > row_scheduled['ON_TIME'] < (datetime.datetime.now() + datetime.timedelta(hours=3)).time() and total_irrigation_volume > liters and not row_scheduled['DONE_FOR_TODAY'] and not row_scheduled['ON_TIME_LENGHT']:
            conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row_scheduled['ON_TIME']) + "\'")
    return 0

def scheduled_irrigation_one_time_liters(device_id, data_device):
    column_scheduled=scheduled_irrigation_one_time_table.select(scheduled_irrigation_one_time_table.c.DEVICE_ID == device_id).execute()
    column_data=data_table.select().where((data_table.c.DEVICE_ID == device_id) & (data_table.c.LAST_LOGIN > datetime.datetime.now() - datetime.timedelta(hours=3))).execute()
    row_devices=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    weather_code=get_wheather_code(row_devices['LATID'], row_devices['LONGIT'])
    print "one_time_liters weather_code:", datetime.datetime.now(), weather_code
    data = {}; i=0; total_irrigation_volume=0
    for row_data in column_data:
        data[i] = {}
        data[i]["ON_OFF_STATE"] = row_data["ON_OFF_STATE"]
        data[i]["WATER_VOLUME"] = row_data["WATER_VOLUME"]
        i+=1      
    for i in range(len(data)):
        if i >= 1 and i < len(data)-1 and (data[i]["ON_OFF_STATE"] == 1 or data[i-1]["ON_OFF_STATE"] == 1) and data[i+1]["WATER_VOLUME"]<data[i]["WATER_VOLUME"]>data[i-1]["WATER_VOLUME"] and data[i]["WATER_VOLUME"] > 0:
            total_irrigation_volume  += float(data[i]["WATER_VOLUME"])
            print "volume", data[i]["WATER_VOLUME"], total_irrigation_volume
        if i == len(data)-1 and data[i]["ON_OFF_STATE"] == 1:
            total_irrigation_volume  += float(data[i]["WATER_VOLUME"])
            print "volume end", data[i]["WATER_VOLUME"], total_irrigation_volume
    total_irrigation_volume  += float(data_device[device_id].WATER_VOLUME_X)
    for row_scheduled in column_scheduled:
        if row_scheduled['ON_TIME_MM'] > 0:
            liters = row_scheduled['ON_TIME_MM']*row_devices["AREA"]
        else:
            liters = row_scheduled['ON_TIME_LITERS']
        if datetime.datetime.now().time() > row_scheduled['ON_TIME'] < (datetime.datetime.now() + datetime.timedelta(hours=3)).time() and total_irrigation_volume < liters and not row_scheduled['DONE_FOR_TODAY'] and not row_scheduled['ON_TIME_LENGHT']:
            if (int(weather_code/100) == 2 or int(weather_code/100) == 5):
                print ("Eso vagy vihar van, ne legyen ontozes")
                print "weather_code:", datetime.datetime.now(), weather_code
                conn.execute("UPDATE scheduled_irrigation_one_time SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row_scheduled['ON_TIME']) + "\'")
            else: return 1
        if datetime.datetime.now().time() > row_scheduled['ON_TIME'] < (datetime.datetime.now() + datetime.timedelta(hours=3)).time() and total_irrigation_volume > liters and not row_scheduled['DONE_FOR_TODAY'] and not row_scheduled['ON_TIME_LENGHT']:
            conn.execute("UPDATE scheduled_irrigation_one_time SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row_scheduled['ON_TIME']) + "\'")
    return 0

def on_disconnect():
    print ('client disconnected')
 
def get_forecast_mm(latitude,longitude):
    forecast = owm.three_hours_forecast_at_coords(latitude, longitude).get_forecast()
    mm_forecast=0
    for i in range(0, 6):
        weather = forecast.get(i)
        if '3h' in weather.get_rain():
            mm_forecast = mm_forecast+weather.get_rain()['3h'];
    return mm_forecast 

def get_past3h_mm(latitude,longitude):
    weather = owm.weather_at_coords(latitude, longitude).get_weather()
    print "rain_last 3h:", datetime.datetime.now(), weather.get_rain()
    if '3h' in weather.get_rain():
        return weather.get_rain()['3h']  
    return 0 

def get_wheather_code(latitude,longitude):
    weather = owm.weather_at_coords(latitude, longitude).get_weather()
    print ("weather code:", datetime.datetime.now(), weather.get_weather_code())
    return weather.get_weather_code()

def temperature_points():
    collumn=devices_table.select().execute()
    for row in collumn:
        forecast_mm=get_forecast_mm(row['LATID'], row['LONGIT'])
        conn.execute("UPDATE devices SET FORECAST_MM = " + str(forecast_mm))
        if(row['IRRIGATION_ON_TEMPERATURE']):            
            temperature_points = row["TEMPERATURE_POINTS"]
            if(row['DAILY_MAX']<15):     temperature_points += 1
            if(15<row['DAILY_MAX']<20):  temperature_points += 2
            if(20<row['DAILY_MAX']<25):  temperature_points += 3
            if(25<row['DAILY_MAX']<30):  temperature_points += 4
            if(30<row['DAILY_MAX']<35):  temperature_points += 5
            if(35<row['DAILY_MAX']):     temperature_points += 6
            if(temperature_points >= row["IRRIGATION_ON_TEMPERATURE"]): 
                conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(temperature_points - row["IRRIGATION_ON_TEMPERATURE"]) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
                if forecast_mm < 5: #ide rakjak valami olyat hogy csak akkor ha kisebb eső várható mint 10mm
                    if (row["IRRIGATION_LENGHT"] != 0): conn.execute("insert into scheduled_irrigation_one_time values(\'" + row["DEVICE_ID"] + "\',\'" + str(row["IRRIGATION_TIME"]) + "\'," + str(row["IRRIGATION_LENGHT"]) + ",0,0,0,1)")           
                    if (row["IRRIGATION_MM"] != 0): conn.execute("insert into scheduled_irrigation_one_time values(\'" + row["DEVICE_ID"] + "\',\'" + str(row["IRRIGATION_TIME"]) + "\',0," + str(row["IRRIGATION_MM"]) + ",0,0,1)")           
                    if (row["IRRIGATION_LITERS"] != 0): conn.execute("insert into scheduled_irrigation_one_time values(\'" + row["DEVICE_ID"] + "\',\'" + str(row["IRRIGATION_TIME"]) + "\',0,0," + str(row["IRRIGATION_LITERS"]) + ",0,1)")           
            else: conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(temperature_points) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")        
        else:   conn.execute("UPDATE devices SET TEMPERATURE_POINTS = 0 where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
    conn.execute("UPDATE devices SET DAILY_MAX = 0")

def find_duplicate(DEVICE_ID,IRRIGATION_TIME,ON_TIME_LENGHT,COMMAND_ID):
    collumn=scheduled_irrigation_one_time_table.select().execute()
    for row in collumn:
        if(row["DEVICE_ID"] != DEVICE_ID):              continue
        if(row["ON_TIME"] != IRRIGATION_TIME):          continue
        if(row["ON_TIME_LENGHT"] != ON_TIME_LENGHT):    continue
        if(row["COMMAND_ID"] != COMMAND_ID):            continue
        return 1
    return 0

def irrigation_on_moisture():
    moisture = 0; loop = 0
    collumn_devices=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    collumn_pairs=pairs_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    for row_devices in collumn_devices:
        if  row_devices["MOISTURE_PERCENT"] == 0: continue       
        device_id=row_devices["DEVICE_ID"]        
        collumn_data = data_table.select(data_table.c.DEVICE_ID == device_id).order_by(desc('LAST_LOGIN')).execute()
        for row_data in collumn_data:
            if(row_data["LAST_LOGIN"] < datetime.datetime.now() - datetime.timedelta(hours=18)): break
            if(12 < row_data["LAST_LOGIN"].hour < 14):
                moisture +=  row_data["MOISTURE"]
                loop += 1
        moisture = moisture/loop
        if(10 < moisture < row_devices["MOISTURE_PERCENT"]):
            for row_pairs in collumn_pairs:
                if (row_devices["DEVICE_ID"] == row_pairs["SENSOR_ID"]):
                    if get_forecast_mm(row_devices['LATID'], row_devices['LONGIT']) < 10:
                        if (row["IRRIGATION_LENGHT"] != 0): conn.execute("insert into scheduled_irrigation_one_time values(\'" + row["DEVICE_ID"] + "\',\'" + str(row["IRRIGATION_TIME"]) + "\'," + str(row["IRRIGATION_LENGHT"]) + ",0,0,0,3)")           
                        if (row["IRRIGATION_MM"] != 0): conn.execute("insert into scheduled_irrigation_one_time values(\'" + row["DEVICE_ID"] + "\',\'" + str(row["IRRIGATION_TIME"]) + "\',0," + str(row["IRRIGATION_MM"]) + ",0,0,3)")           
                        if (row["IRRIGATION_LITERS"] != 0): conn.execute("insert into scheduled_irrigation_one_time values(\'" + row["DEVICE_ID"] + "\',\'" + str(row["IRRIGATION_TIME"]) + "\',0,0," + str(row["IRRIGATION_LITERS"]) + ",0,3)")

print "Server is starting in 3s"
time.sleep(3)
client = mqtt.Client(client_id="MQTT MYSQL Handler")
client.username_pw_set("server",password = "wi7Di5.s+s*")
client.tls_set("/home/odroid/Desktop/server/ca.crt",certfile="/home/odroid/Desktop/server/python.crt",keyfile="/home/odroid/Desktop/server/python.key")
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.connect("localhost",8883)
client.loop_start()

today = datetime.datetime.now().day         #egyszerűbb, de így nem működik pontosan az éjszakán átnyúló öntözés
loop = 0
while 1:
    print "server is alive"
    if loop == 30:
        collumn_devices=devices_table.select().execute()
        for row in collumn_devices:
            collumn_data=data_table.select(data_table.c.DEVICE_ID == row["DEVICE_ID"]).execute().fetchone()
            conn.execute("update data set RAIN_MM = " +  str(get_past3h_mm(row['LATID'], row['LONGIT'])) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\' order by LAST_LOGIN desc limit 1;")
            loop = 0
    if datetime.datetime.now().day is not today and datetime.datetime.now().hour == 0: #and data.values() == []: #ha már elmúlt éjfél
        conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 0")
        conn.execute("DELETE FROM scheduled_irrigation_one_time")
        temperature_points()
        today=datetime.datetime.now().day
    collumn=devices_table.select().execute()     #ez a openweathermap-ról lehúzza az akutális hőmérsékletet és ha ez nagyobb mint aznapi maximum akkor átírja azt erre
    for row in collumn:
        observation = owm.weather_at_coords(row['LATID'], row['LONGIT'])
        T=observation.get_weather().get_temperature('celsius')['temp']
        if (T > row['DAILY_MAX']):
            conn.execute("UPDATE devices SET DAILY_MAX = " + str(T) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
        collumn_hot=hottest_days_irrigation_table.select().execute()     #ez a kánikula alatt állapítja meg kell-e öntözni
        for row_hot in collumn_hot:
            if (row["DEVICE_ID"] ==  row_hot["DEVICE_ID"]):
                if(row_hot["IRRIGATION_ONE_TEMPERATURE"] < T and not find_duplicate(row_hot["DEVICE_ID"],row_hot["IRRIGATION_ONE_TIME"],1,2) 
                and row_hot["IRRIGATION_ONE_CHECK_TIME"].hour == datetime.datetime.now().hour ):
                    conn.execute("insert into scheduled_irrigation_one_time values(\'" + row_hot["DEVICE_ID"] + "\',\'" + str(row_hot["IRRIGATION_ONE_TIME"]) + "\',1,0,2)")
                if(row_hot["IRRIGATION_TWO_TEMPERATURE"] < T and not find_duplicate(row_hot["DEVICE_ID"],row_hot["IRRIGATION_TWO_TIME"],1,2) 
                and row_hot["IRRIGATION_TWO_CHECK_TIME"].hour == datetime.datetime.now().hour):
                    conn.execute("insert into scheduled_irrigation_one_time values(\'" + row_hot["DEVICE_ID"] + "\',\'" + str(row_hot["IRRIGATION_TWO_TIME"]) + "\',1,0,2)")
                if(row_hot["IRRIGATION_THREE_TEMPERATURE"] < T and not find_duplicate(row_hot["DEVICE_ID"],row_hot["IRRIGATION_THREE_TIME"],1,2) 
                and row_hot["IRRIGATION_THREE_CHECK_TIME"].hour == datetime.datetime.now().hour):
                    conn.execute("insert into scheduled_irrigation_one_time values(\'" + row_hot["DEVICE_ID"] + "\',\'" + str(row_hot["IRRIGATION_THREE_TIME"]) + "\',1,0,2)")
     
    print "active threads: "
    print threading.activeCount() 
    if threading.activeCount() < 2: break #mert ezesetben a mosquito szerverhez kapcsolódó szál valószínűleg leállt
    print "loop completed:", loop, "\n"
    loop +=1    
    time.sleep(600)
"""
#bonyolultabb és erőforrásigényesebb, de így az éjszakán átnyúló öntözés is pontos azthiszem   
while 1:
    print "server is alive" 
    print threading.activeCount()
    collumn=scheduled_irrigation_table.select().execute()
    for row in collumn:
        if data.has_key(row["DEVICE_ID"]):
            if not data[row["DEVICE_ID"]].on_time < datetime.datetime.now() < data[row["DEVICE_ID"]].off_time and not row["TODAY"] == datetime.datetime.now().day:
                conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 0" + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
                conn.execute("UPDATE scheduled_irrigation SET TODAY = " + str(datetime.datetime.now().day) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
        elif not row["TODAY"] == datetime.datetime.now().day:
            conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 0" + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
            conn.execute("UPDATE scheduled_irrigation SET TODAY = " + str(datetime.datetime.now().day) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
        print "valami"  
    print threading.activeCount()    
    if threading.activeCount() < 2: break #mert ezesetben a mosquito szerverhez kapcsolódó szál valószínűleg leállt
    time.sleep(600)"""
