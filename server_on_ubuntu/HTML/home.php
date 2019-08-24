<?php
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);
	require_once("session.php");
	
	require_once("class.user.php");
	require_once("class.data.php");
	$auth_user = new USER();
		
	
	$user_id = $_SESSION['user_session'];
	
	$stmt = $auth_user->runQuery("SELECT * FROM users WHERE USER_ID=:user_id");
	$stmt->execute(array(":user_id"=>$user_id));
	
	$userRow=$stmt->fetch(PDO::FETCH_ASSOC);
?>
<!doctype html PUBLIC>
<html>
   <head>
      <meta charset="utf-8">
      <title>Locsoló</title>
      <link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.5/css/bootstrap.min.css">
      <link rel="shortcut icon" href="http://locsol.pe.hu/locsol/favicon.ico">
   </head>
   <style>#szoveg {margin-left:1em;}#chartdiv {width:60%;height:450px;margin-top:40px;margin-right:1%;background:#3f3f4f;color:#ffffff;float:right;padding:5px;opacity:0.9;border-radius:25px;}img.hatter {width:100%;height:100%;position:fixed;top:0;left:0;z-index:-5;}</style>
   <body>
      <img class="hatter" src="http://locsol.pe.hu/locsol/34.jpg">
      <style>body{color:white}</style>
      <div id="szoveg">
      <h1 style="margin-left:30%">Locsolórendszer</h1>
      <div id="chartdiv"></div>
      <br><br>2018.1.29.&nbsp;&nbsp;&nbsp;21:23:43&nbsp;&nbsp;&nbsp;&nbsp; start time: 2018.1.29.&nbsp;&nbsp;&nbsp;21:23:31<br><br> <b><font size="3">Szenzor1 Hőmérséklet: 0.0 °C</font></b><br><font size="2">&nbsp;min: 999.0 °C&emsp;max: -273.0 °C&emsp;avg: nan °C</font><br><b><font size="3">Páratartalom: 0.0%</font></b><br><br><br><span id="allapot">ismeretlen</span><br><br>
      <p title='Ha a locsolási pontok száma eléri a 7 pontot, elindul az automatikus öntözés az előre beálított időpontban. Ezen pontok napi növekedése a 11_13_15 órási középhőmérséklettől függ a következő szabályok szerint:
         •15 °C alatt 1 pont
         •15 °C és 20 °C között  2 pont
         •20 °C és 25 °C között  3 pont
         •25 °C és 30 °C között  4 pont
         •30 °C és 35 °C között  5 pont
         •35 °C felett 6 pont'><br><br><br><br><b>Auto öntözés:</b><br>Locsolo 0 következő öntözés időpontja: --<br>Locsolo 1 következő öntözés időpontja: --<br>11<sup>00</sup>13<sup>00</sup>15<sup>00</sup> órási átlag: 0.00 °C<br>öntözési pontok száma: 7<br>Mai növekedés: 0</p>
      <br>
<nav class="navbar navbar-default navbar-fixed-top">
      <div class="container">
        <div class="navbar-header">
          <button type="button" class="navbar-toggle collapsed" data-toggle="collapse" data-target="#navbar" aria-expanded="false" aria-controls="navbar">
            <span class="sr-only">Toggle navigation</span>
            <span class="icon-bar"></span>
            <span class="icon-bar"></span>
            <span class="icon-bar"></span>
          </button>
          <a class="navbar-brand" href="http://www.codingcage.com">Coding Cage</a>
        </div>
        <div id="navbar" class="navbar-collapse collapse">
          <ul class="nav navbar-nav">
            <li class="active"><a href="http://www.codingcage.com/2015/04/php-login-and-registration-script-with.html">Back to Article</a></li>
            <li><a href="http://www.codingcage.com/search/label/jQuery">jQuery</a></li>
            <li><a href="http://www.codingcage.com/search/label/PHP">PHP</a></li>
          </ul>
          <ul class="nav navbar-nav navbar-right">
            
            <li class="dropdown">
              <a href="#" class="dropdown-toggle" data-toggle="dropdown" role="button" aria-haspopup="true" aria-expanded="false">
			  <span class="glyphicon glyphicon-user"></span>&nbsp;Hi' <?php echo $userRow['user_email']; ?>&nbsp;<span class="caret"></span></a>
              <ul class="dropdown-menu">
                <li><a href="profile.php"><span class="glyphicon glyphicon-user"></span>&nbsp;View Profile</a></li>
                <li><a href="logout.php?logout=true"><span class="glyphicon glyphicon-log-out"></span>&nbsp;Sign Out</a></li>
              </ul>
            </li>
          </ul>
        </div><!--/.nav-collapse -->
      </div>
    </nav>


    <div class="clearfix"></div>


   </body>
   <script>
      var statusDiv = document.getElementById('allapot');
      function refreshStatus() {
      var xmlHttp = new XMLHttpRequest();
      xmlHttp.open( "GET", "/status", false );
      xmlHttp.send(null);
      eval(xmlHttp.responseText);statusDiv.innerHTML = '';var html='';
      for(var i=0; i<locsolok.length; i++) {
      html += '<p><b>Locsoló ' + locsolok[i].id+'</b><font size="3"> ';
      if(locsolok[i].connected == 1) {
      html += '<span class="label label-info">Not Connected</span>';
      } else if(locsolok[i].status == 1) {
      html += '<span class="label label-info">ON</span>';
      } else {
      html += '<span class="label label-info">OFF</span>';
      }
      if(locsolok[i].auto) {
      html += '<span class="label label-info">AUTO ON</span>';
      }
      html +='</font>&emsp;<a href="/locs' + locsolok[i].id + '=on"class="btn btn-success navbar-btn">On</a>';
      html +='&nbsp;<a href="/locs' + locsolok[i].id + '=off"class="btn btn-danger navbar-btn">Off</a>';
      html +='&nbsp;<a href="/locs' + locsolok[i].id + '=auto"class="btn btn-warning navbar-btn">Auto</a><br><font title="min: ' + locsolok[i].min + ' max: ' + locsolok[i].max + '" size="2">Voltage: ';
      html += locsolok[i].voltage;
      html +=' V &nbsp;Hőmérséklet: ';
      html += locsolok[i].temperature;
      html +=' °C &nbsp;Páratartalom: ';
      html += locsolok[i].humidity;
      html +=' %</font></p>';
      }statusDiv.innerHTML=html;}
      refreshStatus();
      var interval=setInterval(refreshStatus,40000);
   </script>
   <script src="http://www.amcharts.com/lib/3/amcharts.js"></script><script src="http://www.amcharts.com/lib/3/serial.js"></script><script src="http://www.amcharts.com/lib/3/themes/dark.js"></script><script>var chartData=generatechartData();
      function generatechartData() {
      var chartData=[];
      var firstDate=new Date();
      var epoch=1504442458;
      var epoch_dt=[];
       var temperature=[];
       var humidity=[ ];
      var locsolo1_temp=[];
      var locsolo1_voltage=[];
      var locsolo2_temp=[];
      var locsolo2_voltage=[];
      var sum=0; for (var i=0; i<epoch_dt.length; i++) {sum+=epoch_dt[i];};var t=epoch-sum;for (var i=0; i<temperature.length; i++) {
      t=t+epoch_dt[epoch_dt.length-i-1];chartData.push({
      date: new Date(t*1000),
      temperature: temperature[temperature.length-i-1],
      humidity: humidity[humidity.length-i-1],
      locs1_temp: locsolo1_temp[locsolo1_temp.length-i-1],
      locs1_voltage: locsolo1_voltage[locsolo1_voltage.length-i-1],
      locs2_temp: locsolo2_temp[locsolo2_temp.length-i-1],
      locs2_voltage: locsolo2_voltage[locsolo2_voltage.length-i-1],
      });
      }
      return chartData;
      }
      var chart=AmCharts.makeChart("chartdiv", {"type": "serial","theme": "dark","marginTop":20,"marginRight": 40,"dataProvider": chartData,
      "legend": {"useGraphSettings": true},
      "valueAxes": [{"axisAlpha": 0,"position": "left"}],
      "graphs": [
      {"id":"g1","balloonText": "[[category]]<br><b><span style='font-size:14px;'>[[temperature]]</span></b>","bullet": "round","bulletSize": 3,   "lineColor": "#d1655d","lineThickness": 1,"negativeLineColor": "#637bb6","type": "smoothedLine","title": "Hőmérséklet","valueField": "temperature"},
      {"id":"g2","balloonText": "[[category]]<br><b><span style='font-size:14px;'>[[humidity]]</span></b>","bullet": "round","bulletSize": 3,   "lineColor": "#5dd165 ","lineThickness": 1,"negativeLineColor": "#637bb6","type": "smoothedLine","title": "Páratartalom","valueField": "humidity"},
      {"id":"g3","balloonText": "[[category]]<br><b><span style='font-size:14px;'>[[locs1_temp]]</span></b>","bullet": "round","bulletSize": 3,   "lineColor": "#d1655d","lineThickness": 1,"negativeLineColor": "#637bb6","type": "smoothedLine","title": "ext.sensor1 temp.","valueField": "locs1_temp"},
      {"id":"g4","balloonText": "[[category]]<br><b><span style='font-size:14px;'>[[locs1_voltage]]</span></b>","bullet": "round","bulletSize": 3,   "lineColor": "#655dd1","lineThickness": 1,"negativeLineColor": "#637bb6","type": "smoothedLine","title": "ext.sensor1 volt.","valueField": "locs1_voltage"},
      {"id":"g5","balloonText": "[[category]]<br><b><span style='font-size:14px;'>[[locs2_temp]]</span></b>","bullet": "round","bulletSize": 3,   "lineColor": "#d1655d","lineThickness": 1,"negativeLineColor": "#637bb6","type": "smoothedLine","title": "ext.sensor2 temp.","valueField": "locs2_temp"},
      {"id":"g6","balloonText": "[[category]]<br><b><span style='font-size:14px;'>[[locs2_voltage]]</span></b>","bullet": "round","bulletSize": 3,   "lineColor": "#655dd1 ","lineThickness": 1,"negativeLineColor": "#637bb6","type": "smoothedLine","title": "ext.sensor2 volt.","valueField": "locs2_voltage"},],
      "chartScrollbar": {"graph":"g1","gridAlpha":0,"color":"#888888","scrollbarHeight":55,"backgroundAlpha":0,"selectedBackgroundAlpha":0.1,"selectedBackgroundColor":"#888888","graphFillAlpha":0,"autoGridCount":true,"selectedGraphFillAlpha":0,"graphLineAlpha":0.2,"graphLineColor":"#c2c2c2","selectedGraphLineColor":"#888888","selectedGraphLineAlpha":1  },
      "chartCursor": {"categoryBalloonDateFormat": "JJ:NN","cursorAlpha": 0,"valueLineEnabled":true,"valueLineBalloonEnabled":true,"valueLineAlpha":0.5,"fullWidth":true},
      "dataDateFormat": "YYYY","categoryField": "date","categoryAxis": {"minPeriod": "10ss","parseDates": true,"minorGridAlpha": 0.1,"minorGridEnabled": true},
      "export": {"enabled": true}});
   </script>
<a href="/settings" target="_blank">settings</a> 
<a href="logout.php?logout=true" target="_blank">Logout</a> 
<?php 
new DATA(); 
DATA::readdata(); 
?>
</html>

