sleep 10
#PYTHONPATH=/home/odroid/.local/lib/python2.7/site-packages
filename="python-$(date +%Y-%V).log"
python2.7 -u /home/odroid/Desktop/server/MySQL/mqtt_mysql_handler.py 2>&1 | while IFS= read -r line; do echo "$(date +%x__%H:%M:%S:%N) $line"; done >> "/home/odroid/logs/$filename" #u unbuffered  output, for logging, add time stuff
