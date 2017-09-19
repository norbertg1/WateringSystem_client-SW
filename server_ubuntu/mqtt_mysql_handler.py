# -*- coding: utf-8 -*-
import MySQLdb
import paho.mqtt.client as mqtt
import datetime
import time
from sqlalchemy import *
from sqlalchemy.orm import *
import ssl
ssl.match_hostname = lambda cert, hostname: True

engine = create_engine("mysql+mysqldb://root:1234@localhost/watering_server?host=localhost?port=3306")
conn = engine.connect()
metadata = MetaData(engine)
Session = sessionmaker(bind=engine)
session = Session()

users_table = Table('users', metadata, autoload=True);
devices_table = Table('devices', metadata, autoload=True);
data_table = Table('data', metadata, autoload=True);
scheduled_irrigation_table = Table('scheduled_irrigation', metadata, autoload=True);

data = {}

class database_data:
    data_table = Table('data', metadata, autoload=True);

    def __init__( self, ID=0, TEMPERATURE=0, HUMIDITY=0, MOISTURE=0, PRESSURE=0, VOLTAGE=0, ON_OFF_STATE=0, TEMPERATURE_POINTS=0,
                  on_time=datetime.datetime.now(), off_time=datetime.datetime.now()):
        self.ID = ID
        self.TEMPERATURE = TEMPERATURE
        self.HUMIDITY = HUMIDITY
        self.MOISTURE = MOISTURE
        self.PRESSURE = PRESSURE
        self.VOLTAGE = VOLTAGE
        self.ON_OFF_STATE = ON_OFF_STATE
        self.TEMPERATURE_POINTS = TEMPERATURE_POINTS
        self.on_time = on_time
        self.off_time = off_time

    def save_database_data(self):
        trans = conn.begin()
        database_data.data_table.insert().execute(DEVICE_ID=self.ID, LAST_LOGIN=time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), TEMPERATURE=self.TEMPERATURE, HUMIDITY=self.HUMIDITY,
                                                    MOISTURE=self.MOISTURE, PRESSURE=self.PRESSURE, VOLTAGE=self.VOLTAGE, ON_OFF_STATE=self.ON_OFF_STATE,
                                                    TEMPERATURE_POINTS = self.TEMPERATURE_POINTS)
        trans.commit()

def on_connect(client, userdata, rc, m):
    client.subscribe("+/TEMPERATURE")
    client.subscribe("+/HUMIDITY")
    client.subscribe("+/MOISTURE")
    client.subscribe("+/PRESSURE")
    client.subscribe("+/VOLTAGE")
    client.subscribe("+/READY_FOR_DATA")
    client.subscribe("+/ON_OFF_STATE")
    client.subscribe("+/FLOWMETER_VOLUME")
    client.subscribe("+/FLOWMETER_VELOCITY")
    client.subscribe("+/AWAKE_TIME")
    client.subscribe("+/END")
    
def on_message(client, userdata, msg):
    device_id=msg.topic[0:(msg.topic).find("/")]
    variable_type=msg.topic[(msg.topic).find("/")+1:len(msg.topic)]
    handle_database(device_id, variable_type, msg.payload)
 
def handle_database(device_id, variable_type, value):
    if (data.has_key(device_id) == 0) and session.query(exists().where(devices_table.c.DEVICE_ID==device_id)).scalar():
        print "device_id logged in:", device_id
        data[device_id] = database_data()
        data[device_id].ID = device_id
    if(data.has_key(device_id)):
        print (variable_type + ':' + value)
        if(variable_type == 'TEMPERATURE'):     data[device_id].TEMPERATURE = value
        if(variable_type == 'HUMIDITY'):        data[device_id].HUMIDITY = value
        if(variable_type == 'MOISTURE'):        data[device_id].MOISTURE = value
        if(variable_type == 'PRESSURE'):        data[device_id].PRESSURE = value
        if(variable_type == 'VOLTAGE'):         data[device_id].VOLTAGE = value
        if(variable_type == 'READY_FOR_DATA'):  send_message_to_device(device_id)
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
                print "deleted"

def send_message_to_device(device_id): #send on_off command
    on_off=read_command_from_database(device_id)
    client.publish(device_id + "/ON_OFF_COMMAND", on_off)
    print 'device', device_id, ':', on_off

def set_delay_sleep_times(device_id):
    client.publish(device_id + "/SLEEP_TIME", "60")
    client.publish(device_id + "/DELAY_TIME", "900")
    client.publish(device_id + "/REMOTE_UPDATE", "1")
    
def read_command_from_database(device_id):
    collumn=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
    if(collumn['ON_COMMAND']): return 1
    if(scheduled_irrigation(device_id)): return 1
    #folytatni
    return 0

def scheduled_irrigation(device_id):
    collumn=scheduled_irrigation_table.select(scheduled_irrigation_table.c.DEVICE_ID == device_id).execute()
    for row in collumn:
        print "percek:"
        print (datetime.datetime.now()-datetime.datetime.combine(datetime.date.today(), row['ON_TIME'])).total_seconds()/60
        if 0 < (datetime.datetime.now()-datetime.datetime.combine(datetime.date.today(), row['ON_TIME'])).total_seconds()/60 < 100 and not row['DONE_FOR_TODAY']:
            conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 1" + " where DEVICE_ID = \'" + device_id + "\' AND ON_TIME = \'" + str(row['ON_TIME']) + "\'")
            data[device_id].on_time = datetime.datetime.now()
            on_time_lenght = row['ON_TIME_LENGHT']
            data[device_id].off_time = (datetime.datetime.combine(datetime.date.today(), data[device_id].on_time.time()) + datetime.timedelta(minutes=on_time_lenght))
            break
    if data[device_id].on_time < datetime.datetime.now() < data[device_id].off_time:    return 1
    return 0

def on_disconnect():
    print ('client disconnected')

client = mqtt.Client()
client.username_pw_set("server",password = "wi7Di5.s+s*")
client.tls_set("ca.crt",certfile="python.crt",keyfile="python.key")
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.connect("localhost",8883)
client.loop_forever()
mqttc.loop_start()

today = datetime.datetime.now().day         #egyszerűbb, de így nem működik pontosan az éjszakán átnyúló öntözés
while 1:
    if datetime.datetime.now().day is not today and data.values() == []: #ha már elmúlt éjfél
        conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 0")
        today=datetime.datetime.now().day

#bonyolultabb és erőforrásigényesebb, de így az éjszakán átnyúló öntözés is pontos    
collumn=scheduled_irrigation_table.select(scheduled_irrigation_table.c.DEVICE_ID == device_id).execute()
for row in collumn:
    if data.has_key(row["DEVICE_ID"]):
        if not data[row["DEVICE_ID"]].on_time < datetime.datetime.now() < data[row["DEVICE_ID"]].off_time and not row["TODAY"] == datetime.datetime.now().day:
            conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 0" + "where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
            conn.execute("UPDATE scheduled_irrigation SET TODAY = " + datetime.datetime.now().day + "where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
    elif not row["TODAY"] == datetime.datetime.now().day:
        conn.execute("UPDATE scheduled_irrigation SET DONE_FOR_TODAY = 0" + "where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
        conn.execute("UPDATE scheduled_irrigation SET TODAY = " + datetime.datetime.now().day + "where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
