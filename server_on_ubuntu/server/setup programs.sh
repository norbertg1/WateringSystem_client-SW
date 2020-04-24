#sudo apt-get install screen -y
#screen
#Ezeket telepiteni, majd screenbol futtatni mindent, igy sokkal biztonsagosabb az SSH telepites
sudo apt-get update -y
sudo apt-get upgrade -y
sudo do-release-upgrade  #Ez csak akkor megy ha minden frissítve van és semmi sehol nem jelez hibát
sudo apt-get dist-upgrade -y
sudo apt-get install mosquitto -y
sudo apt-get install mysql-server -y
export LC_ALL=C
sudo apt-get install python-pip python-dev libmysqlclient-dev -y
pip install --upgrade pip
pip install MySQL-python
pip install paho-mqtt --user
pip install sqlalchemy --user
pip install pyowm
pip install tendo
sudo apt install vsftpd -y
sudo apt-get install php libapache2-mod-php php-mcrypt php-mysql -y
sudo apt-get install libapache2-mod-dnssd
sudo apt-get install mosquitto-clients  #mosquitto_sub -p 1883 -t */* -u anna -P titok;  port, topik neve, név:jelszó

