sleep 3
#PYTHONPATH=/home/odroid/.local/lib/python2.7/site-packages
python2.7 -u /home/odroid/Desktop/server/mqtt_mysql_handler1.py 2>&1 | while IFS= read -r line; do echo "$(date) $line"; done >> /home/odroid/logs/python.log #u unbuffered  output, for logging, add time stuff

