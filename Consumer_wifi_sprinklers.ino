#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti WiFiMulti;
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266httpUpdate.h>
#include <DNSServer.h>
//You will likely need to go in and change the name of this library and any libraries refering to it!
#include <_time.h>
#include <TimeLib.h>
#include <ESP_EEPROM.h>

String tPassword = "";
time_t time_update, lastLogin;
char web_buffer[3800];
char JSON_buff[800];
char timestat[50];
char IPADDRESS[16];


struct myData {
  byte timezone;
  byte schedule[8][7][3];
  char pagePassword[50];
  unsigned int passwordTimeout;
} data;

void EEPROM_Startup() {
  EEPROM.begin(sizeof(myData));
  if (EEPROM.percentUsed() >= 0) {
    EEPROM.get(0, data);
    Serial.println("EEPROM has data from a previous run.");
    Serial.print(EEPROM.percentUsed());
    Serial.println("% of ESP flash space currently used");
    Serial.print("page password:");
    Serial.println(data.pagePassword);
    Serial.print("Password Timeout:");
    Serial.println(data.passwordTimeout);
  } else {
    Serial.println("EEPROM size changed - EEPROM data zeroed - commit() to make permanent");
  }
}

void fullReset() {
  WiFi.disconnect();
  for (int z = 0; z < 8; z++) {
    for (int d = 0; d < 7; d++) {
      for (int n = 0; n < 3; n++) {
        data.schedule[z][d][n] = 0;
      }
    }
  }
  data.pagePassword[0] = char(0);
  data.passwordTimeout = 60;
  data.timezone=0;
  EEPROM_Save();
  delay(2000);
  ESP.reset();
}

String EEPROM_Save() {
  Serial.print(EEPROM.percentUsed());
  EEPROM.put(0, data);
  boolean ok = EEPROM.commit();
  Serial.println((ok) ? "Commit OK" : "Commit Failed");
  return (ok) ? "Commit OK" : "Commit Failed";
}

const byte DNS_PORT = 53;
DNSServer dnsServer;

const char * last_update = "11/07/2019";
ESP8266WebServer server(80);

int x, d, z, h, m, l;
int pins[] = {D0, D1, D2, D3, D4, D5, D6, D7};
long t_running = 0;
char cur_status[100] = "Starting up";


void software_update(String(ip_address)) {
  Serial.println("\nStarting update");
  Serial.println(String(ip_address));
  t_httpUpdate_return ret = ESPhttpUpdate.update(ip_address, 80, "/Consumer_sprinkler.bin");
  Serial.println("UPDATE VALUE");
  Serial.println(ret);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}


void timer(int pin, int len) {
  String AMPM;
  for (int p = 0; p < 8; p++)
  {
    if (pins[p] == pin)
    {
      digitalWrite(pins[p], LOW);
      t_running = now() + len * 60;
      char ftime[5];
      if (isPM(t_running)) {
        AMPM = " PM";
      } else {
        AMPM = " AM";
      }
      sprintf(ftime, "%d:%02d", hourFormat12(t_running), minute(t_running));
      String("Zone " + String(p) + " is Running until " + ftime + AMPM).toCharArray(cur_status, 100);
    }
    else
    {
      digitalWrite(pins[p], HIGH);
    }
  }
}

String check() {
  if (lastLogin + (data.passwordTimeout * 60) < now()) {
    tPassword = "";
  }
  int zstart = 8;
  int zone;
  if (t_running < now())
  {
    for ( zone = 0; zone < 9; zone++)
    {
      if (zone == 8)
      {
        timer(55, 0);
        String("No Zones Running").toCharArray(cur_status, 100);
        return cur_status;
      }
      else if (data.schedule[zone][weekday() - 1][2] > 0 && data.schedule[zone][weekday() - 1][0] == hour() && data.schedule[zone][weekday() - 1][1] == minute())
      {
        break;
      }
    }
    if (zone == 0) timer(D0, data.schedule[zone][weekday() - 1][2]);
    else if (zone == 1) timer(D1, data.schedule[zone][weekday() - 1][2]);
    else if (zone == 2) timer(D2, data.schedule[zone][weekday() - 1][2]);
    else if (zone == 3) timer(D3, data.schedule[zone][weekday() - 1][2]);
    else if (zone == 4) timer(D4, data.schedule[zone][weekday() - 1][2]);
    else if (zone == 5) timer(D5, data.schedule[zone][weekday() - 1][2]);
    else if (zone == 6) timer(D6, data.schedule[zone][weekday() - 1][2]);
    else if (zone == 7) timer(D7, data.schedule[zone][weekday() - 1][2]);
  }
  return cur_status;
}


char * Set_Page() {
  char * page = R"page(
        <style>
      * {
          font-family: sans-serif;
          font-size:2.5vh;
          text-align:center;
      }
      table {
          margin:auto;
          width:80%%;
      }
      td {
          width:50%%;
      }
      th {
          background-image: linear-gradient(#93c5ff, white);
          padding-top:10px;
          border-top: solid black 2px;
          border-radius:50px;
      }
      input,select,button{
          border-radius:20px;
          background:white;
          text-align:center;
      }
      </style>
      <html>
          <button onclick=location='/'>Home</button><br><br>
          <div id=select_zone></div>
          <div id=schedule_zone></div>
          <div id=submit_zone></div>
      </html>
      <script>
      schedule=%s
      var days=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
      for (z=0;z<schedule.length;z++) {
          if (z==0){text='<select onchange=loadup(this.value) id=zone>';}
          text+='<option value='+z+'>Zone '+z+'</option>';
          if (z==7){text+='</select>';select_zone.innerHTML=text}
      }
      schedule_zone.innerHTML='<table border=0>';
      loadup(0);
      submit_zone.innerHTML='</table><br><button  onclick=send()>Update Schedule</button><button onclick=echo()>Clear</button><br><button onclick=Save()>Save Updates</button>';
      function Save() {
        var xhr = new XMLHttpRequest();
        xhr.open('POST','/EEPROM_Save.html',true);
        xhr.send();
      }
      function send() {
          var sub='zone='+document.getElementById('zone').value;
          for (x=0;x<7;x++) {
              sub+='&'+x+'day='+x;
              sub+='&'+x+'hour='+document.getElementById(x+'time').value.split(':')[0];
              sub+='&'+x+'minute='+document.getElementById(x+'time').value.split(':')[1];
              sub+='&'+x+'length='+document.getElementById(x+'length').value;
          }
          var xhr = new XMLHttpRequest();
          xhr.open('POST','/setschedule.html', true);
          xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded; charset=UTF-8');
          xhr.send(sub);
          setTimeout(gohome,1000);
      }
      function loadup(val){
          text='<table border=0>';
          zone=val;
          for (day=0;day<7;day++) {
              text+='<tr><th colspan=2>'+days[day]+'</th></tr><tr><td>Start Time<br><input type=time value='+String(schedule[zone][day][0]).padStart(2,'0')+':'+String(schedule[zone][day][1]).padStart(2,'0')+' id='+day+'time ></td><td>Run Time<br><input type=number  value='+String(schedule[zone][day][2])+' id='+day+'length min=0 max=250></td></tr>';
          }
          schedule_zone.innerHTML=text;
      }
      function echo(){
          console.log("clearing");
          document.querySelectorAll("input[type='time']").forEach(function(ele){ele.value="00:00"});
          document.querySelectorAll("input[type='number']").forEach(function(ele){ele.value=0});
      }
      function gohome() {
        location='';
        }
      </script>)page";

  sprintf(web_buffer, page, Get_JSON());
  return web_buffer;
}

char * Main_Page() {
  char * page = R"page(
      <meta name="viewport" width="device-width" />
    <meta http-equiv="refresh" content="360">
    <style>
    * {
          font-family:sans-serif;
          font-size:2.5vh;
          text-align:center;
    }
    h1{
        border-bottom:2px solid black;
    }
    .head{
        font-weight:bold;
        background-image: linear-gradient(#93c5ff, white);
        padding-top: 10px;
        border-top: solid black 2px;
        border-radius: 50px;
        cursor:pointer;
    }
    button{
     height:80px;
     min-width:30%%;
     border-radius:20px;
     border:solid lightgrey 1px;
     background-image:linear-gradient(0deg,lightgrey,white);
    }
    button:hover{
        background-image:linear-gradient(0deg,rgb(182,182,182),white);
    }
    #Schedule{
        text-align:center;
    }
    .sdata{
        transition-duration:1s;
        font-size:0px;
        }
    </style>
    <html>
    <b>The Sprinkler Page</b><br>
    IP Address: %s<br><br>
    Last Update: %s<br>
    Time: %s <br>
    Status: %s<br><br>
    <button onclick=location='set.html'>Set Schedule</button><button onclick=location='quickrun.html'>Quick Run</button><br>
    <button onclick=location='settings.html'>Settings</button>
    <div id=Schedule>
    <h1>Schedule</h1>
    </div>
    <script>
    schedule=%s
    names=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"]
    days=["","","","","","",""]
    for (zone=0;zone<schedule.length;zone++){
        for (day=0;day<schedule[zone].length;day++){
            if (schedule[zone][day][2] > 0){
                if (schedule[zone][day][0]>12){ side="PM";}else{side="AM";}
                days[day]+="Zone "+String(zone)+" Runs at "+String(schedule[zone][day][0]%%12)+":"+String(schedule[zone][day][1]).padStart(2,'0')+" "+side+" for "+String(schedule[zone][day][2])+" Minutes<br>";
            }
        }
    }
    for (day=0;day<days.length; day++){
        Schedule.innerHTML+="<div class=head onclick=hide(this) >"+names[day]+"</div><div class=sdata>"+days[day]+"</div>";
    }
    function hide(ele){
        if (ele.nextSibling.style.fontSize!="2.5vh"){
            ele.nextSibling.style.fontSize="2.5vh";
        }else{
            ele.nextSibling.style.fontSize="";
        }
    }
    </script>)page";
  char AMPM[3] = "AM";
  if (hour() > 12) {
    AMPM[0] = 'P';
  }
  String IP = WiFi.localIP().toString();
  IP.toCharArray(IPADDRESS, 16);
  sprintf(timestat, "%d:%02d:%02d %s %d/%d/%d", hourFormat12(), minute(), second(), AMPM, month(), day(), year());

  sprintf(web_buffer, page, IPADDRESS, last_update, timestat, cur_status, Get_JSON());
  return web_buffer;
}

char * Quick_Run() {
  char * page = R"page(
    <style>
    * {
        font-family:sans-serif;
        font-size:2vh;
        text-align:center;
    }
    input,select,button{
        border-radius:20px;
        background:white;
        padding-left:10px;
        border:solid 1px lightgrey;
    }
    button{
        min-width:30%;
        height:80px;
        background-image:linear-gradient(0deg,lightgrey,white);
        border:solid 1px lightgrey;
    }
    </style>
    <html>
        <button onclick=location='/'>Home</button><br><br>
        <div id=here></div>
    </html>
    <script>
        text='<select id=zone>';
        for (x=0;x<8;x++) {
          text+='<option value='+x+'>Zone '+x+'</option>';
        }
        text+='</select>';
        text+='<input type=number id=quickrun max=60 min=0 value=15><br><br><br><button onclick=send(1)>Start</button><button onclick=send(0)>Stop</button>';
        here.innerHTML=text;
        function send(n) {
          var sub='zone='+document.getElementById('zone').value;
          var xhr = new XMLHttpRequest();
          xhr.open('POST','/quick', true);
          xhr.setRequestHeader('Content-Type','application/x-www-form-urlencoded; charset=UTF-8');
          if (n == 0) {
            xhr.send('stop=true');}
          else if (n == 1) {
            xhr.send(sub + '&length='+document.getElementById('quickrun').value+'&stop=false')
          }
        }
    </script>)page";
  return page;
}

char * savePassword() {
  char * page = R"page(<style>
  *{  
    font-size:3vh;
    }
    td{
        font-size:3vh; 
        text-align:center; 
        }
    button,input[type=submit]{
         height:80px;
         min-width:30%;            
         border-radius:20px;
         border:solid lightgrey 1px;
         background-image:linear-gradient(0deg,lightgrey,white);
    }                                                         
    </style>
<html style="text-align:center"> 
<form method=post style="
    position: relative;
    margin: auto;
    top: 25vh;
" action="spwset.html"><table style=margin:auto border=0> 
<tr><td colspan=2> 
<button onclick=location="/">Home</button>
</tr><td>                                                                                                                                                   
<tr>                                                                                                                                                            
<td colspan=2><b>Set Up Page Password</b></td>                                                                                                          
</tr>                                                                                                                                                       
<tr>                                                                                                                                                            
<td>Password:</td>                                                                                                                                          
<td><input id=p1 onchange=check() style="border-radius:15px" name="pword" type="password" required></td>                                                
</tr>
<tr>                                                                                                                                                            
<td>Reenter Password:</td><td><input id=p2 onchange=check() style="border-radius:15px" name="pword2" type=password required></td>                       
</tr>                                                                                                                                                      
<tr>                                                                                                                                                            
<td style="text-align:center;" colspan=2><input id=but type="submit" disabled></td>                                                                     
</tr>                                                                                                                                                      
</form>                                                                                                                                                     
<form action=spwset.html method=post>                                                                                                                       
<input name=clear style=display:none type=number value=1>                                                                                                   
<tr>                                                                                                                                                            
<td colspan=2><input type=submit value="Remove Password"></td>                                                                                          
</tr>                                                                                                                                                       
</table>
</form>
</html>
<script>
function check(){
    console.log("check");
    if (p1.value && p1.value==p2.value)
        but.disabled=false;
    else but.disabled=true;
}
</script>)page";
  return page;
}

char * slogin() {
  Serial.println("Slogin page!");
  char * page = R"page(
  <style>
    * {font-size:3vh;}
    button,input[type=submit]{
     height:80px;
     min-width:30%;
     border-radius:20px;
     border:solid lightgrey 1px;
     background-image:linear-gradient(0deg,lightgrey,white);
    }
  </style>
  <html style="text-align:center">
  <form method=post style="
    position: relative;
    margin: auto;
    top: 25vh;" 
    action="slogin.html">
Password:<input style="top:50vh;border-radius:15px" name="pword" type="password">
<br><input type="submit">
</form>
</html>)page";
  return page;
}

char * Get_JSON() {
  String s = "[";
  for (byte z = 0; z < 8; z++) {
    if (z == 0) s += "[";
    else s += ",[";
    for (byte d = 0; d < 7; d++) {
      if (d == 0) {
        s += "[";
      }
      else {
        s += ",[";
      }
      s += String(data.schedule[z][d][0]) + "," + String(data.schedule[z][d][1]) + "," + String(data.schedule[z][d][2]);
      s += "]";
    }
    s += "]";
  }
  s += "]";
  s.toCharArray(JSON_buff, 800);
  return JSON_buff;
}

char * settings_page() {
  char * page = R"page(<style>
    * {
        font-family: sans-serif;
        }
    body {
      font-size:3vh;
      text-align:center;
      }
    button,input[type=submit]{
      font-size:3vh;
      height:80px;
     min-width:30%;
     border-radius:20px;
     border:solid lightgrey 1px;
     background-image:linear-gradient(0deg,lightgrey,white);
    }
input,select{                     
  width:50%;
  font-size:3vh;
          border-radius:20px;
          text-align:center;
      }
    .header{
        font-weight:bold;
        font-size:4vh;
    }
</style>                
<html>
<h1>SETTINGS PAGE</h1>
<button onclick=location="/">Home</button>
    <br><br>
    <div class=header>Update Firmware</div>
    <form method = 'post' action = 'update'>
    <input type ='text'  name ='ip_address'  value='jeffshomesite.com'><br>
    <input type ='submit' value='update'></form>
    <div class=header>Change Password</div>
    <button onclick=location="spwset.html">Change Password</button><br><br>
    <div class=header>Change Password Timeout</div>
    <form method = 'post' action="settings.html">
    <input type=number max=65534 name=ptimeout>Minutes<br>
    <input type='submit'></form>
    <div class=header>Full Reset</div>
    <button onclick=location="fullReset">Reset</button><br><br>                                                                                             </html>                                     
  
  )page";
  return page;
}

void update_time() {
  configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  delay(5000);
  configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  time_t t = time(NULL);
  setTime(t);
  time_update = now() + (72 * 24 * 3600);
}

void setup() {
  Serial.begin(74880);
  WiFiManager wifiManager;
  wifiManager.autoConnect("Wifi-Sprinkler-setup");
  Serial.println("connected!");
  delay(1000);
  configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  delay(5000);
  configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  time_t t = time(NULL);
  setTime(t);
  time_update = now() + (72 * 24 * 3600);
  EEPROM_Startup();
  for (int p = 0; p < 8; p++)
  {
    pinMode(pins[p], OUTPUT);
    Serial.println("setting up pin " + String(pins[p]) + "=" + String(p));
    digitalWrite(pins[p], HIGH);
  }
  int n;
  // Wait for connection
  WiFi.hostname("wifi sprinklers");
  Serial.println("Connecting...");
  while (WiFiMulti.run() != WL_CONNECTED) {
    n++;
    delay(500);
    Serial.print(".");
    if (n % 10 == 0) Serial.println(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.mode(WIFI_AP_STA);
  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAP("WiFi_Sprinkler");
  WiFi.softAPConfig(apIP, apIP, netMsk);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);




  //////////////WEBServer/////////////////

  server.on("/quickrun.html", [] () {
    if (String(data.pagePassword) != "") {
      if (tPassword != String(data.pagePassword)) {
        server.send(200, "text,html", slogin());
        delay(1000);
      } else server.send(200, "text,html", Quick_Run());
    } else {
      server.send(200, "text,html", Quick_Run());
      delay(1000);
    }
  });
  server.on("/fullReset", [] () {
    if (String(data.pagePassword) != "") {
      if (tPassword != String(data.pagePassword)) {
        server.send(200, "text,html", slogin());
        delay(1000);
      } else {
        server.send(200, "text,html", "Factory Resetting!");
        delay(1000);
        fullReset();
      }
    } else {
      server.send(200, "text,html", "Factory Resetting!");
      delay(1000);
      fullReset();
    }
  });
  server.on("/spwset.html", [] () {
    if (String(data.pagePassword) != "") {
      if (tPassword != String(data.pagePassword)) {
        server.send(200, "text,html", slogin());
        delay(1000);
      } else {
        if (server.hasArg("pword")) {
          unsigned int x;
          for (x = 0; x < server.arg("pword").length(); x++) {
            data.pagePassword[x] = server.arg("pword")[x];
          }
          data.pagePassword[server.arg("pword").length()] = char(0);
          Serial.println("Saving password:" + server.arg("pword"));
          server.send(200, "text,html", EEPROM_Save());
          delay(1000);
          Serial.println("Password saved as:" + String(data.pagePassword));
        } else if (server.hasArg("clear")) {
          data.pagePassword[0] = char(0);
          EEPROM_Save();
          server.send(200, "text,html", Main_Page());
          delay(1000);
        } else {
          server.send(200, "text,html", savePassword());
          delay(1000);
        }
      }
    } else {
      if (server.hasArg("pword")) {
        unsigned int x;
        for (x = 0; x < server.arg("pword").length(); x++) {
          data.pagePassword[x] = server.arg("pword")[x];
        }
        data.pagePassword[server.arg("pword").length()] = char(0);
        Serial.println("Saving password:" + server.arg("pword"));
        server.send(200, "text,html", EEPROM_Save());
        delay(1000);
        Serial.println("Password saved as:" + String(data.pagePassword));
      } else if (server.hasArg("clear")) {
        data.pagePassword[0] = char(0);
        EEPROM_Save();
        server.send(200, "text,html", Main_Page());
        delay(1000);
      } else {
        server.send(200, "text,html", savePassword());
        delay(1000);
      }
    }
  });
  server.on("/slogin.html", [] () {
    Serial.println("got /slogin.html request");
    if (server.hasArg("pword")) {
      if (server.arg("pword") == String(data.pagePassword)) {
        tPassword = server.arg("pword");
        lastLogin = now();
        Serial.println("tpassword:" + tPassword);
        Serial.println("Last Login:" + String(lastLogin));
        Serial.print("passwords match?:");
        Serial.println(tPassword == String(data.pagePassword));
        server.send(200, "text/html", Main_Page());
        delay(1000);
      } else {
        server.send( 200, "text,html", slogin());
        delay(1000);
      }
    } else {
      server.send(200, "text,html", slogin());
      delay(1000);
    }
  });
  server.on("/quick", [] () {
    if (server.arg("stop") == "true") {
      t_running = now();
    }
    else {
      Serial.println("Quick run " + server.arg("zone") + " " + server.arg("length"));
      timer(pins[server.arg("zone").toInt()], server.arg("length").toInt());
    }
    server.send(200, "text/html", Main_Page());
    delay(1000);
  });

  server.on("/", []() {
    Serial.println("got / request");
    if (String(data.pagePassword) != "") Serial.println("password required");
    else Serial.println("no password required");
    if (String(data.pagePassword) != "") {
      if (tPassword != String(data.pagePassword)) {
        server.send(200, "text,html", slogin());
        delay(1000);
      } else {
        server.send(200, "text/html", Main_Page());
        delay(1000);
      }
    } else {
      Serial.println("sending main page");
      server.send(200, "text/html", Main_Page());
      delay(1000);
    }
  });
  server.on("/settings.html", []() {
    if (String(data.pagePassword) != "") {
      if (tPassword != String(data.pagePassword)) {
        server.send(200, "text,html", slogin());
        delay(1000);
      }
      if (server.hasArg("ptimeout")) {
        data.passwordTimeout = server.arg("ptimeout").toInt();
        Serial.print("new timeout:");
        Serial.println(data.passwordTimeout);
        EEPROM_Save();
        server.send(200, "text/html", settings_page());
        delay(1000);
      } else server.send(200, "text/html", settings_page());
    } else {
      server.send(200, "text/html", settings_page());
      delay(1000);
    }
  });
  server.on("/set.html", []() {
    if (String(data.pagePassword) != "") {
      if (tPassword != String(data.pagePassword)) {
        server.send(200, "text,html", slogin());
        delay(1000);
      } else server.send(200, "text/html", Set_Page());
    } else {
      server.send(200, "text/html", Set_Page());
      delay(1000);
    }
  });
  server.on("/EEPROM_Save.html", []() {
    EEPROM_Save();
    server.send(200, "text/html", Main_Page());
    delay(1000);
  });
  server.on("/setschedule.html", []() {
    for (int wday = 0; wday < 7; wday++) {
      if (server.arg(String(wday) + "hour") != "" && server.arg(String(wday) + "minute") != "" && server.arg(String(wday) + "length") != "") {
        z = (server.arg("zone").toInt());
        d = (server.arg(String(wday) + "day").toInt());
        h = (server.arg(String(wday) + "hour").toInt());
        m = (server.arg(String(wday) + "minute").toInt());
        l = (server.arg(String(wday) + "length").toInt());
        data.schedule[z][d][0] = h;
        data.schedule[z][d][1] = m;
        data.schedule[z][d][2] = l;
      }
    }
    for (int z = 0; z <= 7; z++) {
      Serial.println("\nZone=" + String(z));
      for (int d = 0; d < 7; d++) {
        Serial.println("\nDay=" + String(d));
        for (int h = 0; h < 3; h++) {
          Serial.print(String(data.schedule[z][d][h]) + " ");
        }
      }
    }
    delay(1000);
  });
  server.on("/update", []() {
    if (server.arg("ip_address") == "") {
      server.send(200, "text,html", "<html style='text-align: center; font-size:300%'><b>UPDATING from jeffshomesite.com!!!!<br>Page will reload in a few seconds...</b></html><script> setTimeout(gohome,15000); function gohome() {location='/';}</script>");
      delay(1000);
    } else {
      server.send(200, "text,html", "<html style='text-align: center; font-size:300%'><b>UPDATING from " + String(server.arg("ip_address")) + "!!!!<br>Page will reload in a few seconds...</b></html><script> setTimeout(gohome,15000); function gohome() {location='/';}</script>");
      delay(1000);
    }
    if (server.arg("ip_address") == "") {
      Serial.println("\nUpdate address is empty: defaulting to jeffshomesite.com");
      software_update("jeffshomesite.com");
    } else {
      Serial.println(server.arg("ip_address"));
      software_update(server.arg("ip_address"));
    }
    delay(100);
  });
  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  if (Serial.available()) {
    char r = Serial.read();
    if (r == 'R') {
      fullReset();
    }
    else if (r == 'T') {
      unsigned int n = Serial.parseInt();
      data.passwordTimeout = n;
      EEPROM_Save();
    }
    else Serial.println(Serial.readString());
  }
  if (now() > time_update) {
    update_time();

  }
  check();
  delay(100);
  dnsServer.processNextRequest();
  server.handleClient();
}
