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

owm = pyowm.OWM('80c4722573ef8abe4b03228d9465fe09') 

data = {}

class database_data:
    data_table = Table('data', metadata, autoload=True);

    def __init__( self, DEVICE_NAME=0, DEVICE_ID=0, TEMPERATURE=0, HUMIDITY=0, MOISTURE=0, PRESSURE=0, VERSION=0, RST_REASON=0, WATER_VOLUME=0, WATER_VELOCITY=0, VOLTAGE=0, ON_OFF_STATE=0, TEMPERATURE_POINTS=0, AWAKE_TIME=0, RSSI=0,
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
        self.WATER_VELOCITY = WATER_VELOCITY
        self.VOLTAGE = VOLTAGE
        self.ON_OFF_STATE = ON_OFF_STATE
        self.TEMPERATURE_POINTS = TEMPERATURE_POINTS
        self.on_time = on_time
        self.off_time = off_time
        self.AWAKE_TIME = AWAKE_TIME
        self.RSSI = RSSI


    def save_database_data(self):
        trans = conn.begin()
        database_data.data_table.insert().execute(DEVICE_ID=self.DEVICE_ID, DEVICE_NAME = self.DEVICE_NAME, LAST_LOGIN=time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), 
                                                    TEMPERATURE=self.TEMPERATURE, HUMIDITY=self.HUMIDITY, MOISTURE=self.MOISTURE, PRESSURE=self.PRESSURE, VERSION=self.VERSION,
                                                    RST_REASON=self.RST_REASON, WATER_VOLUME=self.WATER_VOLUME, WATER_VELOCITY=self.WATER_VELOCITY, VOLTAGE=self.VOLTAGE,
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
    client.subscribe("+/FLOWMETER_VELOCITY")
    client.subscribe("+/AWAKE_TIME")
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
        if(variable_type == 'FLOWMETER_VOLUME'):    data[device_id].WATER_VOLUME = value
        if(variable_type == 'FLOWMETER_VELOCITY'):  data[device_id].WATER_VELOCITY = value
        if(variable_type == 'VOLTAGE'):             data[device_id].VOLTAGE = value
        if(variable_type == 'RSSI'):                data[device_id].RSSI = value
        if(variable_type == 'AWAKE_TIME'):          data[device_id].AWAKE_TIME = value
        if(variable_type == 'READY_FOR_DATA'):      send_message_to_device(device_id)            
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

def send_message_to_device(device_id): #send on_off command
    on_off=read_command_from_database(device_id)
    client.publish(device_id + "/ON_OFF_COMMAND", on_off)
    print 'Command for the device', device_id, ':', on_off
    set_delay_sleep_times(device_id)

def set_delay_sleep_times(device_id):
    collumn=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    #print collumn['SLEEP_TIME']
    client.publish(device_id + "/SLEEP_TIME", str(collumn['SLEEP_TIME']))
    client.publish(device_id + "/DELAY_TIME", str(collumn['DELAY_TIME']))
    client.publish(device_id + "/REMOTE_UPDATE", str(collumn['REMOTE_UPDATE']))
    
def read_command_from_database(device_id):
    collumn=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    if(collumn['ON_COMMAND']): return 1
    if(scheduled_irrigation(device_id)): return 1
    #if(scheduled_irrigation_one_time(device_id)): return 1
    #folytatni ha ez a funkcio 1est ad vissza az bekapcsolja az adott eszközön az öntözést
    return 0

def scheduled_irrigation(device_id):
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

def scheduled_irrigation_one_time(device_id):
    collumn=scheduled_irrigation_one_time_table.select(scheduled_irrigation_one_time_table.c.DEVICE_ID == device_id).execute()
    for row in collumn:
        if 0 < (datetime.datetime.now()-datetime.datetime.combine(datetime.date.today(), row['ON_TIME'])).total_seconds()/60 < 100 and not row['DONE_FOR_TODAY']:
            conn.execute("UPDATE scheduled_irrigation_one_time SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row['ON_TIME']) + "\'")
            data[device_id].on_time = datetime.datetime.now()
            on_time_lenght = row['ON_TIME_LENGHT']
            data[device_id].off_time = (datetime.datetime.combine(datetime.date.today(), data[device_id].on_time.time()) + datetime.timedelta(minutes=on_time_lenght))
            break
    if data[device_id].on_time < datetime.datetime.now() < data[device_id].off_time:    return 1
    return 0

def on_disconnect():
    print ('client disconnected')

def temperature_points():
    collumn=devices_table.select().execute()
    for row in collumn:
        if(row['IRRIGATION_ON_TEMPERATURE']):            
            if(row['DAILY_MAX']<15):     conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(int(row["TEMPERATURE_POINTS"]) + 1) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
            if(15<row['DAILY_MAX']<20):  conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(int(row["TEMPERATURE_POINTS"]) + 2) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
            if(20<row['DAILY_MAX']<25):  conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(int(row["TEMPERATURE_POINTS"]) + 3) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
            if(25<row['DAILY_MAX']<30):  conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(int(row["TEMPERATURE_POINTS"]) + 4) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
            if(30<row['DAILY_MAX']<35):  conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(int(row["TEMPERATURE_POINTS"]) + 5) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
            if(35<row['DAILY_MAX']):     conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(int(row["TEMPERATURE_POINTS"]) + 6) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
            if(row["TEMPERATURE_POINTS"] >= 7): 
                conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(row["TEMPERATURE_POINTS"] - 7) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
                conn.execute("insert into scheduled_irrigation_one_time values(\'" + row["DEVICE_ID"] + "\',\'" + str(row["IRRIGATION_TIME"]) + "\'," + str(row["IRRIGATION_LENGHT"]) + ",0)")           
        else:   conn.execute("UPDATE devices SET TEMPERATURE_POINTS = 0 where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")

print "Server is starting in 10s"
time.sleep(10)
client = mqtt.Client(client_id="MQTT MYSQL Handler")
client.username_pw_set("server",password = "wi7Di5.s+s*")
client.tls_set("/home/odroid/Desktop/server/ca.crt",certfile="/home/odroid/Desktop/server/python.crt",keyfile="/home/odroid/Desktop/server/python.key")
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.connect("localhost",8883)
client.loop_start()

today = datetime.datetime.now().day         #egyszerűbb, de így nem működik pontosan az éjszakán átnyúló öntözés
while 1:
    print "server is alive" 
    if datetime.datetime.now().day is not today: #and data.values() == []: #ha már elmúlt éjfél
        conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 0")
        conn.execute("DELETE FROM scheduled_irrigation_one_time")
        today=datetime.datetime.now().day
    temperature_points()
    collumn=devices_table.select().execute()     #ez a openweathermap-ról lehúzza az akutális hőmérsékletet és ha ez nagyobb mint aznapi maximum akkor átírja azt erre
    for row in collumn:
        observation = owm.weather_at_coords(row['LATID'], row['LONGIT'])
        T=observation.get_weather().get_temperature('celsius')['temp']
        if (T > row['DAILY_MAX']):
            print ("valami")
            conn.execute("UPDATE devices SET DAILY_MAX = " + str(T) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
    print "active threads: "  
    print threading.activeCount() 
    if threading.activeCount() < 2: break #mert ezesetben a mosquito szerverhez kapcsolódó szál valószínűleg leállt
    print "loop completed\n"
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
