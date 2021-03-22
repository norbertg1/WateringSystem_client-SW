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
from threading import Timer
import subprocess

print "MQTT_MySQL handler starting."
if len(sys.argv) is 1: 
	me = singleton.SingleInstance() # will sys.exit(-1) if other instance is running

#Ha ez a sor ki van kommentezve akkor ez a process amit a crontabe 15 precenként elindít kiütök egymást
#ellenben így lehet manuálisan elindítani ezt a kódot módosítás után mert megszünteti az előző futását
#ha argumentummal inditom parancssorból akkor nem kell kikommentezni, kényelmesebb

engine = create_engine("mysql+mysqldb://python_agent:U.iEw+aLMN+NM.*@localhost/watering_server?host=localhost?port=3306", pool_recycle=3600, pool_pre_ping=True)
conn = engine.connect()
metadata = MetaData(engine)
Session = sessionmaker(bind=engine)
session = Session()
users_table = Table('users', metadata, autoload=True);
devices_table = Table('devices', metadata, autoload=True);
data_table = Table('data', metadata, autoload=True);
scheduled_irrigation_table = Table('scheduled_irrigation', metadata, autoload=True);
scheduled_irrigation_one_time_table = Table('scheduled_irrigation_one_time', metadata, autoload=True);
scheduled_irrigation_result_table = Table('scheduled_irrigation_result', metadata, autoload=True);
hottest_days_irrigation_table = Table('hottest_days_irrigation', metadata, autoload=True);
pairs_table = Table('pairs', metadata, autoload=True);
forecast_mm_limit = 5
delta_hours = 4
ABSOLUT_MAXIMUM_ON_TIME = 60 * 60 #default 60*60 = 1 hour
owm = pyowm.OWM('80c4722573ef8abe4b03228d9465fe09') #openweathermap

MOISTURE_HOURS_WINDOW1 = 10; MOISTURE_HOURS_WINDOW2 = 18
data = {}
MINIMUM_VALVE_OPEN_VOLTAGE = 3.1

class database_data:
	data_table = Table('data', metadata, autoload=True);

	def __init__( self, DEVICE_ID=0, TEMPERATURE=0, HUMIDITY=0, MOISTURE=0, PRESSURE=0, VERSION=0, RST_REASON=0, WATER_VOLUME=0, WATER_VOLUME_X=0, WATER_VELOCITY=0, MM=0, VOLTAGE=0, ON_OFF_STATE=0, TEMPERATURE_POINTS=0, AWAKE_TIME=0, AWAKE_TIME_X=0, RSSI=0, STOP_FLAG = 0,
				  on_time=datetime.datetime.now(), off_time=datetime.datetime.now()):
S
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
		self.STOP_FLAG = STOP_FLAG

	def save_database_data(self):
		trans = conn.begin()
		database_data.data_table.insert().execute(DEVICE_ID=self.DEVICE_ID, LAST_LOGIN=time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()), 
													TEMPERATURE=self.TEMPERATURE, HUMIDITY=self.HUMIDITY, MOISTURE=self.MOISTURE, PRESSURE=self.PRESSURE, VERSION=self.VERSION,
													RST_REASON=self.RST_REASON, WATER_VOLUME=self.WATER_VOLUME, WATER_VELOCITY=self.WATER_VELOCITY, MM=self.MM, VOLTAGE=self.VOLTAGE,
													ON_OFF_STATE=self.ON_OFF_STATE, TEMP_OPENWEATHER = self.TEMP_OPENWEATHER, RAIN_MM=self.RAIN_MM, RSSI=self.RSSI, AWAKE_TIME=self.AWAKE_TIME)
		trans.commit()

	def get_area(self):
		column=devices_table.select(devices_table.c.DEVICE_ID == self.DEVICE_ID).execute().fetchone()
		return column['AREA']
	
	def end(self):
		print "Local time:" + time.strftime("%Y-%b-%d %H:%M:%S", time.localtime()) + "\n-----------------SLEEP------SLEEP------SLEEP------SLEEP------SLEEP------SLEEP------SLEEP--------------------------------\n\n"
		data[self.DEVICE_ID].save_database_data()
		del data[self.DEVICE_ID]; #ezt már nem tudom mi, helyetesitem
		#if not data[self.DEVICE_ID].on_time < datetime.datetime.now() < data[self.DEVICE_ID].off_time:
			#del data[self.DEVICE_ID]; #ezt már nem tudom mi	Törölni 2018.11.26

def on_connect(client, userdata, rc, m):
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
		print "LOGGED IN DEVICE_ID:", device_id
		data[device_id] = database_data()
		#Timer(120.0, data[device_id].end).start()
		data[device_id].DEVICE_ID = device_id
		print "get area:", data[device_id].get_area()
	if(data.has_key(device_id)):
		print (variable_type + ':' + value)
		if(variable_type == 'TEMPERATURE'):         data[device_id].TEMPERATURE = value
		if(variable_type == 'HUMIDITY'):            data[device_id].HUMIDITY = value
		if(variable_type == 'MOISTURE'):            data[device_id].MOISTURE = value
		if(variable_type == 'PRESSURE'):            data[device_id].PRESSURE = value
		if(variable_type == 'VERSION'):             data[device_id].VERSION = value
		if(variable_type == 'RST_REASON'):          data[device_id].RST_REASON = value
		if(variable_type == 'FLOWMETER_VELOCITY'):  data[device_id].WATER_VELOCITY = value
		if(variable_type == 'VOLTAGE'):             data[device_id].VOLTAGE = value
		if(variable_type == 'RSSI'):                data[device_id].RSSI = value
		if(variable_type == 'AWAKE_TIME'):          data[device_id].AWAKE_TIME = value						#ezt az értéket a kapcsolat utolsó pillanatában küldi el a kliens
		if(variable_type == 'AWAKE_TIME_X'):        data[device_id].AWAKE_TIME_X = value					#ezt az értéket a kapcsolat első pillanatában küldi el a kliens
		if(variable_type == 'READY_FOR_DATA'):      send_message_to_device(device_id, data)
		if(variable_type == 'FLOWMETER_VOLUME_X'):  data[device_id].WATER_VOLUME_X = value; get_mm(device_id, value)	#ezt az értéket a kapcsolat utolsó pillanatában küldi el a kliens
		if(variable_type == 'FLOWMETER_VOLUME'):   	data[device_id].WATER_VOLUME = value; 	get_mm(device_id, value)	#ezt az értéket a kapcsolat első pillanatában küldi el a kliens
		if(variable_type == 'ON_OFF_STATE'):                #ha be volt kapcsolva a locsolás (amit az adatbázisból olvasok ki) és most olyan érték jön,
			collumn = data_table.select(data_table.c.DEVICE_ID == device_id).order_by(desc('LAST_LOGIN')).execute().fetchone()
			if collumn is not None:
				if collumn['ON_OFF_STATE'] == 1 and int(value) == 0:   #hogy pont most kapcsolt ki, akkor elmentem a kikapcsolási időpontot az adatbázisba
					conn.execute("UPDATE devices SET LAST_ON_TIME = \'"+ time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()) + "\' where DEVICE_ID = \'" + device_id + "\'")
			data[device_id].ON_OFF_STATE = value         
		if(variable_type == 'END'):                     
			print "Local time:" + time.strftime("%Y-%b-%d %H:%M:%S", time.localtime()) + "\n-----------------SLEEP------SLEEP------SLEEP------SLEEP------SLEEP------SLEEP------SLEEP--------------------------------\n\n"
			data[device_id].save_database_data()
			del data[device_id]; #ezt már nem tudom mi, helyetesitem
			#if not data[device_id].on_time < datetime.datetime.now() < data[device_id].off_time:
			#	del data[device_id]; #ezt már nem tudom mi	Törölni 2018.11.26

def get_mm(device_id, value):
	column=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
	if(column['AREA'] != 0 ):   data[device_id].MM = float(value)/column['AREA']
	print "FLOWMETER_MM:", data[device_id].MM

def send_message_to_device(device_id, data): #send on_off command
	print "Read from database starting here (MySQL, OWM, etc...)!"
	on_off=read_command_from_database(device_id, data)
	print "Read completed, now we send back the answer to the device and waiting for ON_OFF_STATE..."
	client.publish(device_id + "/ON_OFF_COMMAND", on_off)
	print 'Command for the device', device_id, ':', on_off
	set_delay_sleep_times(data, device_id)

def set_delay_sleep_times(data, device_id):
	collumn=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
	if (data[device_id].STOP_FLAG == 1):	client.publish(device_id + "/SLEEP_TIME", str(60))		#Ez azért van, hogy öntözés után 60 másodperc múlva ébredjen fel, és ellenőrizze, hogy nem-e  kell mégegy öntözést lefuttatni. Mert ha 3600 másdopercesek
	else: client.publish(device_id + "/SLEEP_TIME", str(collumn['SLEEP_TIME']))						#a SLEEP timeok akkor elég hosszú késés kialakulhat.
	client.publish(device_id + "/DELAY_TIME", str(collumn['DELAY_TIME']))
	client.publish(device_id + "/REMOTE_UPDATE", str(collumn['REMOTE_UPDATE']))
	client.publish(device_id + "/REMOTE_LOG", str(collumn['REMOTE_LOG']))
	
def read_command_from_database(device_id, data): 
	collumn=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
	weather_code=get_wheather_code(device_id, data)
	if(collumn['ON_COMMAND']): return collumn['ON_COMMAND']
	if(scheduled_irrigation(device_id, data, weather_code)): return 1
	return 0

def scheduled_irrigation(device_id, data_device, weather_code):		#data_device-ot jó lenne a jövőben csak data-nak hívni azthiszem.
	print "READING COMMAND: scheduled_irrigation, Local time: ", datetime.datetime.now().strftime('%H:%M:%S') 
#↓↓↓↓↓ csak a feltételeknek MEGFELELŐ sorokat választom ki, innét lejebb ↓↓↓↓
	scheduled_table=scheduled_irrigation_table.select().where((scheduled_irrigation_table.c.DEVICE_ID == device_id)  #Az ON_LENGTH feltétel azért kell, hogy a done_for_today funkcióba
		& (scheduled_irrigation_table.c.TODAY == 0)
		& (((datetime.datetime.now().strftime('%Y-%m-%d') == scheduled_irrigation_table.c.ON_DATE) & (scheduled_irrigation_table.c.END_DATE == None))       #ne írja be azokat az IRRIGATION_ID tervezett locsolásokat a scheduled_irrigation_result táblába ami nem való oda
				| ((datetime.datetime.now().strftime('%Y-%m-%d') >= scheduled_irrigation_table.c.ON_DATE) & (scheduled_irrigation_table.c.END_DATE != None)))
		& ((datetime.datetime.now().strftime('%Y-%m-%d') <= scheduled_irrigation_table.c.END_DATE) | (scheduled_irrigation_table.c.END_DATE == None)) 
		& (datetime.datetime.now().strftime('%H:%M:%S') > scheduled_irrigation_table.c.ON_TIME) 
		& (scheduled_irrigation_table.c.ON_TIME > (datetime.datetime.now() - datetime.timedelta(hours=delta_hours)).time())) #itt az execute-t később rakom hozzá
	row_scheduled_result = scheduled_irrigation_result_table.select().where((scheduled_irrigation_result_table.c.DEVICE_ID == device_id)
		& ((scheduled_irrigation_result_table.c.REAL_DATETIME) > (datetime.datetime.now() - datetime.timedelta(hours=delta_hours)))
		& (scheduled_irrigation_result_table.c.RESULT == 0)).execute().fetchone()
#	CSAK A FELTÉTELEKNEK MEGFELELŐ SOROKKAL DOLGOZOM TOVÁBB !!!
#↓↓↓↓ Megnézem van-e folyamatban levő öntözés vagy delta_hours órán belül megszakadt öntözés ↓↓↓↓
	if row_scheduled_result is None:	#Ha nincs a feltételeknek megfelelő sor a scheduled_irrigation_result táblában 
		row_scheduled_irrigation_table = scheduled_table.execute().fetchone() 
		print " scheduled_irrigation_result table is None"
		if row_scheduled_irrigation_table is not None: #akkor berakok egyet a scheduled_irrigation táblából (Ha jelenleg nem kell öntözni akkor ez üres és kiugrok a funkcióból)
			print " INSERT INTO scheduled_irrigation_result . New scheduled irrigation starting... "
			print "   conn.execute: "
			print conn.execute("INSERT INTO scheduled_irrigation_result values(\'" + str(row_scheduled_irrigation_table["IRRIGATION_ID"]) + "\',\'" + str(device_id) + "\',\'"
				 + str(time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())) + "\'," + "0" + "," + "0" + "," + "0" + "," + "0" + ")")
			if ((int(weather_code/100) == 2 or int(weather_code/100) == 5) and not row_scheduled_irrigation_table['COMMAND_ID'] == -1):	#IDŐJÁRÁS
				print " Az öntözés eső vagy vihar végett meghiúsul! RESULT= 2"
				conn.execute("UPDATE scheduled_irrigation_result SET RESULT = 2 WHERE IRRIGATION_ID = \'" + str(row_scheduled_irrigation_table["IRRIGATION_ID"]) + "\';")
				conn.execute("UPDATE scheduled_irrigation SET TODAY = 1 WHERE IRRIGATION_ID = \'" + str(row_scheduled_irrigation_table["IRRIGATION_ID"]) + "\';")
				return 0
			if data[device_id].VOLTAGE < MINIMUM_VALVE_OPEN_VOLTAGE: #2019.4.19 és tesztelem hogy meg-e van a minimum akksifeszültség, Öntözés közben ez a feltétel nem fut le csak az elején!
				print " Az öntözés a szelep akkumulátor alacsony feszültsége végett meghiúsul! RESULT= 4"
				conn.execute("UPDATE scheduled_irrigation_result SET RESULT = 4 WHERE IRRIGATION_ID = \'" + str(row_scheduled_irrigation_table["IRRIGATION_ID"]) + "\';")
				conn.execute("UPDATE scheduled_irrigation SET TODAY = 1 WHERE IRRIGATION_ID = \'" + str(row_scheduled_irrigation_table["IRRIGATION_ID"]) + "\';")
				return 0
			return 1
	else:	#Ha  van a feltételeknek megfelelő sor a scheduled_irrigation_result táblában akkor frissítem abban az értékeket (locsolási idő, vízmennyiség, stb..)
		for row_scheduled_irrigation_table in scheduled_table.execute():
			if row_scheduled_irrigation_table['IRRIGATION_ID'] == row_scheduled_result['IRRIGATION_ID']:
				SCHEDULED_LENGTH = row_scheduled_irrigation_table['LENGTH']
				SCHEDULED_LITERS = get_liters(device_id, row_scheduled_irrigation_table['LITERS'], row_scheduled_irrigation_table['MM'])	#mm to liter konverziót végez
				IRRIGATION_ID = str(row_scheduled_irrigation_table["IRRIGATION_ID"])
		#↓↓↓↓ Innét frissítti az adatbázisban az eltelt öntözési időhosszt és a vízmennyiséget ↓↓↓↓
				if row_scheduled_result["REAL_LENGTH"] < float(data_device[device_id].AWAKE_TIME_X):	#Ha az öntözés nem szakad meg, vagyis nem kapcsol ki a kliens közben (pl. szar wifi végett)
					CURRENT_LENGTH = float(data_device[device_id].AWAKE_TIME_X)
					CURRENT_LITERS = get_liters(device_id, float(data_device[device_id].WATER_VOLUME_X), float(data[device_id].MM))		#mm to liter konverziót végez
					print " Irrigation_result_table UPDATE"
					conn.execute("UPDATE scheduled_irrigation_result SET REAL_LENGTH = " + str(CURRENT_LENGTH) + ", REAL_LITERS = " 
						+ str(data[device_id].WATER_VOLUME_X) + ", REAL_MM = " + str(float(data[device_id].MM)) 
						+ " WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\' and DATE(REAL_DATETIME) = \'" + (datetime.datetime.now().strftime('%Y-%m-%d')) + "\';")
				else:	#Ha az öntözés megszakad (pl. szar wifi végett)
					CURRENT_LENGTH = row_scheduled_result["REAL_LENGTH"] + float(data_device[device_id].AWAKE_TIME_X)
					CURRENT_LITERS = get_liters(device_id, row_scheduled_result["REAL_LITERS"] + float(data_device[device_id].WATER_VOLUME_X), row_scheduled_result["REAL_MM"] + float(data[device_id].MM))		#mm to liter konverziót végez
					print " Az öntözés megszakadt idő: ", CURRENT_LENGTH, "\bs liter:", CURRENT_LITERS, "\n Irrigation_result_table UPDATE"
					conn.execute("UPDATE scheduled_irrigation_result SET REAL_LENGTH = " + str(CURRENT_LENGTH) + ", REAL_LITERS = " 
						+ str(CURRENT_LITERS) + ", REAL_MM = " + str(row_scheduled_result["REAL_MM"] + float(data[device_id].MM)) + " WHERE IRRIGATION_ID = \'" 
						+ IRRIGATION_ID + "\' and DATE(REAL_DATETIME) = \'" + (datetime.datetime.now().strftime('%Y-%m-%d')) + "\';")
		#↓↓↓↓ Innét értékelődik ki, hogy meddig kell még öntözni ↓↓↓↓
				if	CURRENT_LENGTH > ABSOLUT_MAXIMUM_ON_TIME:	#MAXIMUM locsolóidő elérése végett áll le az öntözés
					print " Az öntözés kikapcsol, mert elérte a MAXIMALIS öntözési időt: ",CURRENT_LENGTH, "\bs liter: ", CURRENT_LITERS, "\bL", "\n RESULT= 3"
					conn.execute("UPDATE scheduled_irrigation_result SET RESULT = 3 WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\' and DATE(REAL_DATETIME) = \'" + (datetime.datetime.now().strftime('%Y-%m-%d')) + "\';")
					conn.execute("UPDATE scheduled_irrigation SET TODAY = 1 WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\';")
					data_device[device_id].STOP_FLAG = 1
					return 0					
				if ((int(weather_code/100) == 2 or int(weather_code/100) == 5) and not row_scheduled_irrigation_table['COMMAND_ID'] == -1):	#IDŐJÁRÁS De ez ide talán nem is kell, mert az öntözés elején már tesztelem ezt. Talán ha gyorsan változik az időjárás??
					print " Az öntözés kikapcsol mert eso vagy vihar van, eddigi öntözési idő: ", CURRENT_LENGTH, "\bs liter: ", CURRENT_LITERS, "\bL", "\n RESULT= 2"
					conn.execute("UPDATE scheduled_irrigation_result SET RESULT = 2 WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\' and DATE(REAL_DATETIME) = \'" + (datetime.datetime.now().strftime('%Y-%m-%d')) + "\';")
					conn.execute("UPDATE scheduled_irrigation SET TODAY = 1 WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\';")
					data_device[device_id].STOP_FLAG = 1
					return 0
				if SCHEDULED_LENGTH < CURRENT_LENGTH and SCHEDULED_LENGTH > 0:	#IDŐHOSSZ SZERINTI ÖNTÖZÉS
					DONE = row_scheduled_irrigation_table['DONE'] + 1
					print " Az öntözés kikapcsol, mert elérte a tervezett öntözési időt: ",CURRENT_LENGTH, "\bs liter: ", CURRENT_LITERS, "\bL", " DONE= ", DONE, " RESULT= 1"
					conn.execute("UPDATE scheduled_irrigation_result SET RESULT = 1 WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\' and DATE(REAL_DATETIME) = \'" + (datetime.datetime.now().strftime('%Y-%m-%d')) + "\';")
					conn.execute("UPDATE scheduled_irrigation SET TODAY = 1, DONE = " + str(DONE) + " WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\';")
					data_device[device_id].STOP_FLAG = 1
					return 0
				if SCHEDULED_LITERS < CURRENT_LITERS and SCHEDULED_LITERS > 0:	#VÍZMENNYISÉG SZERINTI ÖNTÖZÉS
					DONE = row_scheduled_irrigation_table['DONE'] + 1
					print " Az öntözés kikapcsol, mert elérte a tervezett öntözési mennyiséget: ",CURRENT_LENGTH, "\bs liter: ", CURRENT_LITERS, "\bL", " DONE= ", DONE, " RESULT= 1"
					conn.execute("UPDATE scheduled_irrigation_result SET RESULT = 1 WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\' and DATE(REAL_DATETIME) = \'" + (datetime.datetime.now().strftime('%Y-%m-%d')) + "\';")
					conn.execute("UPDATE scheduled_irrigation SET TODAY = 1, DONE = " + str(DONE) + " WHERE IRRIGATION_ID = \'" + IRRIGATION_ID + "\';")
					data_device[device_id].STOP_FLAG = 1
					return 0
		#↓↓↓↓ Ha bekapcsolo parancs van	↓↓↓↓			
				if SCHEDULED_LENGTH > CURRENT_LENGTH: #Azért nem csak row_scheduled_result["ON_REAL_LENGTH"], mert itt még nem frissül az adatbázis és a régi értéket tartalmazza
					print " Az időzített öntözés folytatódk! idő:", CURRENT_LENGTH, "\bs tervezett idő: ", SCHEDULED_LENGTH, "\bs vízmennyiség: ", CURRENT_LITERS, "\bL Tervezett vízmennyiség: ", SCHEDULED_LITERS, "\bL"
					return 1
				if SCHEDULED_LITERS > CURRENT_LITERS: #Azért nem csak row_scheduled_result["ON_REAL_LITERS"], mert itt még nem frissül az adatbázis és a régi értéket tartalmazza
					print " A mennyiségi öntözés folytatódk! idő:", CURRENT_LENGTH, "\bs tervezett idő: ", SCHEDULED_LENGTH, "\bs vízmennyiség: ", CURRENT_LITERS, "\bL tervezett vízmennyiség: ", SCHEDULED_LITERS, "\bL"
					return 1

def get_liters(device_id, liters, mm):
	row_devices_table=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
	if liters > 0: return liters
	liters = mm*row_devices_table["AREA"]
	print " convert MM:", mm, " to liters:", liters, "\bL"
	return liters

def on_disconnect():
	print ('client disconnected')
 
def get_forecast_mm(latitude,longitude,time_block):
	forecast = owm.three_hours_forecast_at_coords(latitude, longitude).get_forecast()
	mm_forecast=0
	for i in range(0, time_block):
		weather = forecast.get(i)
		if '3h' in weather.get_rain():
			mm_forecast = mm_forecast+weather.get_rain()['3h'];
	return mm_forecast 

def get_past3h_mm(weather):
	print "Figyelni van-e valaha eso elorejelzes nálunk!!!!!! rain_last 3h:", datetime.datetime.now(), weather.get_rain()
	if '3h' in weather.get_rain():
		return weather.get_rain()['3h']  
	return 0 

def get_wheather_code(device_id, data):
	print "Get weather code"
	collumn=devices_table.select(devices_table.c.DEVICE_ID == device_id).execute().fetchone()
	weather = owm.weather_at_coords(collumn['LATID'], collumn['LONGIT']).get_weather()
	data[device_id].RAIN_MM = get_past3h_mm(weather)
	data[device_id].TEMP_OPENWEATHER = weather.get_temperature('celsius')['temp']
	print " Weather_code:", weather.get_weather_code(), " Temperature:", weather.get_temperature('celsius')['temp'], "°C Rain:", data[device_id].RAIN_MM, "\bmm for lat:",  collumn['LATID'], "long:", collumn['LONGIT']
	return weather.get_weather_code()

def temperature_points():
	for row in devices_table.select().execute():
		forecast_mm=get_forecast_mm(row['LATID'], row['LONGIT'],6)
		conn.execute("UPDATE devices SET FORECAST_MM = " + str(forecast_mm)) #éjjel kiszámolom, hogy a követéző  azthiszem 3x6 órában mennyi eső várható
		if(row['IRRIGATION_ON_TEMPERATURE']):            
			temperature_points = row["TEMPERATURE_POINTS"]
			if(row['DAILY_MAX']<5):     temperature_points += 0
			if(5<=row['DAILY_MAX']<15):  temperature_points += 1
			if(15<=row['DAILY_MAX']<20):  temperature_points += 2
			if(20<=row['DAILY_MAX']<25):  temperature_points += 3
			if(25<=row['DAILY_MAX']<30):  temperature_points += 4
			if(30<=row['DAILY_MAX']<35):  temperature_points += 5
			if(35<=row['DAILY_MAX']):     temperature_points += 6
			if(temperature_points >= row["IRRIGATION_ON_TEMPERATURE"]): 
				conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(temperature_points - row["IRRIGATION_ON_TEMPERATURE"]) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
				if forecast_mm < forecast_mm_limit: #ide rakjak valami olyat hogy csak akkor ha kisebb eső várható mint forecast_mm_limit mm
					if (row["IRRIGATION_LENGTH"] != 0): conn.execute("insert into scheduled_irrigation values(0,\'" + row["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," 
						+ "default" + ",\'" + str(row["IRRIGATION_TIME"]) + "\'," + str(row["IRRIGATION_LENGTH"]) + ",0,0,0,0,1)")           
					if (row["IRRIGATION_MM"] != 0):     conn.execute("insert into scheduled_irrigation values(0,\'" + row["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," 
						+ "NULL" + ",\'" + str(row["IRRIGATION_TIME"]) + "\',0,0," + str(row["IRRIGATION_MM"]) + ",0,0,1)")           
					if (row["IRRIGATION_LITERS"] != 0): conn.execute("insert into scheduled_irrigation values(0,\'" + row["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," 
						+ "NULL" + ",\'" + str(row["IRRIGATION_TIME"]) + "\',0," + str(row["IRRIGATION_LITERS"]) + ",0,0,0,1)")           
			else: conn.execute("UPDATE devices SET TEMPERATURE_POINTS = " + str(temperature_points) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")        
		else:   conn.execute("UPDATE devices SET TEMPERATURE_POINTS = 0 where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
	conn.execute("UPDATE devices SET DAILY_MAX = 0")

#ezt a funkciot ejfelkor futtatom le. Ekkor megállapítja hogy az előző napi időablakban (MOISTURE_HOURS_WINDOW1 MOISTURE_HOURS_WINDOW2 - délután 10 és 18 óra között)
#kisebb nedvességet-e tartalmazott a talaj mint a beálított szint amennyiben kevesebb volt ezaz érték, beállít egy öntözési időpontot
def irrigation_on_moisture():
	pairs_table_selected=pairs_table.select().execute()
	for row_pairs in pairs_table_selected: #végigmegyek a pairs táblán
		row_devices=devices_table.select(devices_table.c.DEVICE_ID == row_pairs["VALVE_ID"]).execute().fetchone() #kiválasztom azt a sort devices táblából ahol a device_ID egyezik a pairsban levővel
		if row_devices["MOISTURE_PERCENT"] == 0: continue		#és megnézem hogy arra a szelepre van-e beállítva a devicesban öntözési nedvesség ha nincs akkor ugrok tovább
		collumn_data = data_table.select().where((data_table.c.DEVICE_ID == row_pairs["SENSOR_ID"])		#Ehhez a szelephez tartozó SENSOR_ID-vel dolgozok tovabb, mert az méri a nedvességet
		& (data_table.c.LAST_LOGIN > (datetime.datetime.now() - datetime.timedelta(hours=delta_hours)))).order_by(desc('LAST_LOGIN')).execute()
		moisture = 0; loop = 0                
		for row_data in collumn_data:
			print row_data["DEVICE_ID"], row_data["LAST_LOGIN"], row_data["MOISTURE"]
			if(MOISTURE_HOURS_WINDOW1 < row_data["LAST_LOGIN"].hour < MOISTURE_HOURS_WINDOW2):
				moisture +=  row_data["MOISTURE"] #össze van számolva az adott időablakban mért talajnedvesség
				loop += 1
		if loop > 0: moisture = moisture/loop
		print row_devices["DEVICE_ID"],"\b_moisture:", moisture
		if(10 < moisture < row_devices["MOISTURE_PERCENT"]):
			if get_forecast_mm(row_devices['LATID'], row_devices['LONGIT'], 6) < forecast_mm_limit:
				print "Irrigation on moisture added on device", row_devices["DEVICE_ID"], row_devices["IRRIGATION_TIME"], "időhossz: ", str(row_devices["IRRIGATION_LENGTH"]), "\bs vízmennyiség: ", str(row_devices["IRRIGATION_LITERS"]), "\bL mm: ", str(row_devices["IRRIGATION_MM"])
				if (row_devices["IRRIGATION_LENGTH"] != 0):
					conn.execute("insert into scheduled_irrigation values(null,\'" + row_devices["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," + "NULL" + ",\'" + str(row_devices["IRRIGATION_TIME"]) + "\'," + str(row_devices["IRRIGATION_LENGTH"]) + ",0,0,0,0,3)")           
				if (row_devices["IRRIGATION_LITERS"] != 0):
					conn.execute("insert into scheduled_irrigation values(null,\'" + row_devices["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," + "NULL" + ",\'" + str(row_devices["IRRIGATION_TIME"]) + "\',0," + str(row_devices["IRRIGATION_LITERS"]) + ",0,0,0,3)")
				if (row_devices["IRRIGATION_MM"] != 0):
					conn.execute("insert into scheduled_irrigation values(null,\'" + row_devices["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," + "NULL" + ",\'" + str(row_devices["IRRIGATION_TIME"]) + "\',0,0," + str(row_devices["IRRIGATION_MM"]) + ",0,0,3)")

def hottest_days_irrigation():
#↓↓↓↓ Innét kánikula alatt állapítom meg kell-e öntözni ↓↓↓↓
	for row in	devices_table.select().execute():
		hottest_days_irrigation_table_select=hottest_days_irrigation_table.select().execute()
		for row_hot in hottest_days_irrigation_table_select:
			if (row["DEVICE_ID"] == row_hot["DEVICE_ID"]):
				if(row_hot["IRRIGATION_ONE_TEMPERATURE"] < row["DAILY_MAX"] and find_duplicate(row_hot["DEVICE_ID"],21) and row_hot["IRRIGATION_ONE_CHECK_TIME"].hour == datetime.datetime.now().hour ):
					print "kánikula_1"
					conn.execute("insert into scheduled_irrigation values(null,\'" + row_hot["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," + "NULL" + ",\'" + str(row_hot["IRRIGATION_ONE_TIME"]) + "\',120,0,0,0,0,21)")
				if(row_hot["IRRIGATION_TWO_TEMPERATURE"] < row["DAILY_MAX"] and find_duplicate(row_hot["DEVICE_ID"],22) and row_hot["IRRIGATION_TWO_CHECK_TIME"].hour == datetime.datetime.now().hour):
					print "kánikula_2"
					conn.execute("insert into scheduled_irrigation values(null,\'" + row_hot["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," + "NULL" + ",\'" + str(row_hot["IRRIGATION_TWO_TIME"]) + "\',120,0,0,0,0,22)")
				if(row_hot["IRRIGATION_THREE_TEMPERATURE"] < row["DAILY_MAX"] and find_duplicate(row_hot["DEVICE_ID"],23) and row_hot["IRRIGATION_THREE_CHECK_TIME"].hour == datetime.datetime.now().hour):
					print "kánikula_3"
					conn.execute("insert into scheduled_irrigation values(null,\'" + row_hot["DEVICE_ID"] + "\',\'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'," + "NULL" + ",\'" + str(row_hot["IRRIGATION_THREE_TIME"]) + "\',120,0,0,0,0,23)")

def find_duplicate(DEVICE_ID, COMMAND_ID):
	scheduled_irrigation_table_select=scheduled_irrigation_table.select().where((scheduled_irrigation_table.c.DEVICE_ID == DEVICE_ID) 
		& (datetime.datetime.now().strftime('%Y-%m-%d') == scheduled_irrigation_table.c.ON_DATE)).execute()
	for row in scheduled_irrigation_table_select:
		if row["COMMAND_ID"] == COMMAND_ID: return 0
	return 1

def at_loop():
	collumn_devices=devices_table.select().execute()
	for row in collumn_devices:
	#↓↓↓↓ ez a openweathermap-ról lehúzza az akutális hőmérsékletet és ha ez nagyobb mint aznapi maximum akkor átírja azt erre ↓↓↓↓
		observation = owm.weather_at_coords(row['LATID'], row['LONGIT'])
		T=observation.get_weather().get_temperature('celsius')['temp']
		if (T > row['DAILY_MAX']):
			conn.execute("UPDATE devices SET DAILY_MAX = " + str(T) + " where DEVICE_ID = \'" + row["DEVICE_ID"] + "\'")
	
def midnight_tasks():
	print "Midnight Tasks"
	conn.execute("UPDATE scheduled_irrigation SET TODAY = 0 WHERE END_DATE >= \'" + datetime.datetime.now().strftime('%Y-%m-%d') + "\'")
	temperature_points()
	irrigation_on_moisture()

#↓↓↓↓ main loop ↓↓↓↓
print "Server is starting in 3s"
time.sleep(3)
client = mqtt.Client(client_id="MQTT MYSQL Handler")
client.username_pw_set("python_agent",password = "wi7i5.s+s*rtkgso")
client.tls_set("/home/odroid/Desktop/server/MySQL/ca.crt",certfile="/home/odroid/Desktop/server/MySQL/python.crt",keyfile="/home/odroid/Desktop/server/MySQL/python.key")
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.connect("localhost",8883)
client.loop_start()

today = datetime.datetime.now().day
loop = 0
while 1:
	print "************************************************************************************************\nserver is alive"
	if loop == 1:
		at_loop()
		loop = 0
	if datetime.datetime.now().day is not today and datetime.datetime.now().hour == 0: #and data.values() == []: #ha már elmúlt éjfél
		midnight_tasks()
		today=datetime.datetime.now().day
	hottest_days_irrigation()
	print "active threads: ", threading.activeCount() 
	if threading.activeCount() < 2: break #mert ezesetben a mosquito szerverhez kapcsolódó szál valószínűleg leállt
	print "loop completed:", loop, "\n************************************************************************************************\n"
	loop +=1    
	time.sleep(600)
	if datetime.datetime.now().weekday() is 6 and datetime.datetime.now().hour is 0 and datetime.datetime.now().minute<10:
		print "============================================================================================================================="
		print "========================================== END OF THE WEEK==================================================================="
		print "============================================================================================================================="
		subprocess.Popen(["sh", "mqtt_mysql_handler.sh"] + sys.argv[1:])
		break