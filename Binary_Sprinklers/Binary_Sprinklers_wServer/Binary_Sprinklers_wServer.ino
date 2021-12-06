#include <ESP8266WiFi.h>
#include "DNSServer.h"
#include <ESP8266WebServer.h>
#include <time.h>
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h>
#include <TimeLib.h>
#include <ESP_EEPROM.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include "LittleFS.h"
#include <ESP8266mDNS.h>




const uint8_t DNS_PORT = 53;          // Capture DNS requests on port 53
DNSServer dnsServer;              // Create the DNS object
ESP8266WebServer  server(80);

String tPassword = "";
time_t time_update, lastLogin = 0;
char IPADDRESS[16];
uint8_t pins[] = {D0, D1, D2, D3, D4, D5, D6, D7, 60};
time_t activeTime = 0;
uint8_t Status = 255;
time_t passwordTimeout = 0;
time_t checkWifi = 0;

struct qr {
  uint8_t zone;
  time_t runTime;
} quickRun;

struct schedulePoint {
  uint8_t days;
  uint8_t zone;
  uint8_t runTime;
  int16_t start;
};

struct myData {
  char networkSSID[50] = "";
  char networkPW[50] = "";
  char serverUName[25] = "";
  char serverPW[25] = "";
  schedulePoint schedule[64];
  char pagePassword[25] = "";
  uint8_t passwordTimeout;
  char ssid[25] = "Wifi_Sprinklers";
  int ZipCode = 0;
  bool hidden = false;
  float smartDelay = 0;
  char zoneNames[8][25] = {"zone1", "zone2", "zone3", "zone4", "zone5", "zone6", "zone7", "zone8"};
} data;

bool connect();
char * dataDump(char * buff);
String EEPROM_Save();
bool serverRecv();
char bigbuff[1500];
signed long memory = 0;
time_t lastMessage, local;
char ServerMessage[100];
String Version = " Complied:" + String(__DATE__);

//webpages

//const char getTime[] PROGMEM = R"EOF(<script> function getTime(){ var d=new Date; var tz=d.getTimezoneOffset()/-60; var UTC=d.getTime()/1000; return "UTC="+String(UTC)+"&tz="+String(tz); } function sendTime(){; req=new XMLHttpRequest(); req.open("POST","setTime.html",true); req.setRequestHeader('Content-type', 'application/x-www-form-urlencoded'); req.send(getTime()); } sendTime(); </script>)EOF";

const char loginPage[] PROGMEM = R"EOF(<meta name="viewport" content="width=device-width, initial-scale=1" user-scalable=no><style>html{font-size:4vw;}@media screen and (min-width: 700px){html{font-size:35px;}}</style><html><div style=position:absolute;top:25%;left:25%;text-align:center;>Login<form method=post action="saveData.html"><input type=text style=display:none value="login" name=id><input type=password placeholder=password name=pw><input type=submit></form></div></html>)EOF";

const char  styleSheet[] PROGMEM = R"EOF(<style> @media screen and (min-width: 700px){html{font-size:35px;}}html{font-size:3vw;}*{font-size:1rem;font-family:sans-serif;}@media screen and (min-width:700px){html{font-size:20px;}}form{text-align:center;}button,input[type=submit]{margin:5px;border-radius:10px;border: solid black 3px;font-weight:bold;}.popout{position:fixed;top:10vh;left:10vw;right:10vw;background:white;text-align:center;border:solid black 5px;border-radius:10px;box-shadow: 5px 5px 25px;padding:10px;}.sortable{cursor:pointer;}table{margin:auto;border-collapse:collapse;}.header{background:rgb(240,240,240);border-bottom:solid 1px lightgray;text-align:left;cursor:pointer;transition:.5s;}.header:hover{transition:.5s;padding-left:1rem;}.datatable{border-collapse:collapse;width:90%;}input,svg,select{cursor:pointer;text-align:center;text-align-last:center;margin-bottom:5px;}.datatable td{text-align:center;width:33%;padding-left:1rem;padding-right:1rem;text-align:center;border-bottom:solid 1px rgb(140,140,140);}input:hover{background-color:rgb(240,240,240);}#stable select{border:none;text-align:center;}select:hover{background-color:rgb(240,240,240);}.datatable input{text-align:center;border:none;}.datatable select{border:none;}input[type=number]{width:3rem;}.title{background:rgb(240,240,240);border-right:solid 1px black;border-left:solid 1px black;width:33%;border-top:2px solid black;border-bottom:2px solid black;}.divhead{margin-bottom:.25rem;margin-top:1.5rem;font-weight:bold;text-decoration:underline;text-align:center;width:100%;}.right{text-align:right}</style>)EOF";

const char  settingsPage[] PROGMEM = R"EOF(<meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" type="text/css" href="stylesheet.css"><script src="Data.js"></script><object style=width:100%; data="header.html"></object><html style=text-align:center;><div style=text-align:center;font-weight:bold;font-size:1.5rem;>Settings</div><div class=divHead>Zone Names</div><div id=zoneNamesForm style=text-align:center;></div><div class=divHead>Set Password</div><button onclick=setPassword.style.display="" >Set Password</button><form id=setPassword method=POST class=popout onsubmit=s(this,event) style=display:none ><div class=divHead>Password Settings</div>Password:<input type=password id=pw1 name=pw1><br>Password:<input type=password id=pw2 name=pw2><br>Timeout:<input type=number id=timeout name=timeout><br><input type=submit  onclick=this.parentElement.style.display="none" value=Save><button onclick=this.parentElement.style.display="none">Cancel</button></form><div class=divHead>Wifi Settings</div><div style=text-align:center;id=wifi><form onsubmit=s(this,event) id="AP"  method=POST>Access Point Visibility:<select name=visibility><Option value=0>Visible</option><option value=1>Hidden</option></select><br>SSID:<input type=text name=ssid id=ssid><br><input type=submit value=Save></form></div><div class=divHead>Remote Server</div><table><form id=remoteServer onsubmit=s(this,event) method=POST><tr><td class="right">UserName:</td><td><input id=serverUName name=serverUName type=text placeholder="UserName"></td></tr><tr><td class="right">Password:</td><td><input name=serverPW id=serverPW type=password placeholder=Password><svg style=margin-bottom:-8px width="2rem" viewBox="0 0 24 24" onmousedown=serverPW.type="text" onmouseup=serverPW.type="password"><path d="M12 6.5c3.79 0 7.17 2.13 8.82 5.5-1.65 3.37-5.02 5.5-8.82 5.5S4.83 15.37 3.18 12C4.83 8.63 8.21 6.5 12 6.5m0-2C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zm0 5c1.38 0 2.5 1.12 2.5 2.5s-1.12 2.5-2.5 2.5-2.5-1.12-2.5-2.5 1.12-2.5 2.5-2.5m0-2c-2.48 0-4.5 2.02-4.5 4.5s2.02 4.5 4.5 4.5 4.5-2.02 4.5-4.5-2.02-4.5-4.5-4.5z"/></svg></td></tr></table><input type=submit value=Save></form><div class=divHead>Smart Features</div><form id=smartFeature onsubmit=s(this,event) method=POST><table><tr><td class="right">Location:</td><td id=Town></td></tr><tr><td class="right">Total Rain:</td><td id=Rain></td></tr><tr><td class="right">Forcast Rain:</td><td id=Frain></td></tr><tr><td class="right">Zip Code:</td><td><input style=width:5rem type=text id=zipcode name=zipcode></td></tr><tr><td class="right">Smart Delay(in):</td><td><input id=smartRain name=smartRain type=number step="0.01" min=0></td></tr></table><input type=submit value=Save></form><div class=divHead>Firmware Update</div><form id=Update onsubmit=s(this,event)  method=POST><input type=text placeholder="Update Server" value="jeffshomesite.com" name=source><br><input value=Update type=submit onclick='alert("Device will update and reboot")'></form><div class=divHead>Reboot Sprinkler Controller</div><form id=reboot onsubmit=s(this,event)><input type=submit value=Reboot></form><div class=divHead>Erase All Saved Data</div><form id=reset onsubmit=s(this,event) ><input type=submit value="Full Reset"></form><html><script>serverUName.value=ServerUNameVal;ssid.value=SSID;Town.innerText=LocationVal;Rain.innerText=RainVal;Frain.innerText=FrainVal;zipcode.value=ZipCodeVal;smartRain.value=smartRainVal;function pairEncode(fd){fd=Array.from(fd.entries());f=fd[0][0]+"="+fd[0][1];for (var x=1;x< fd.length;x++){f += "&"+fd[x][0]+"="+fd[x][1];}return f;}ssid.value=SSID;zn();function s(ele,eve){eve.preventDefault();if (ele.id == "reset"){if (!confirm("This will erase all saved data! Continue?")){return false;}}else if (ele.id == "setPassword"){if (pw1.value != pw2.value){alert("Passwords Must Match");return false;}}fd=new FormData(ele);fd.set("id",ele.id);data=pairEncode(fd);var r=new XMLHttpRequest;r.onreadystatechange=function(){if (this.readyState == 4 && this.status == 200){alert(this.responseText);}};r.open("POST","saveData.html",true);r.setRequestHeader('Content-type','application/x-www-form-urlencoded');r.send(data);return false;}function zn(){var n='<form id=zoneNames onsubmit=s(this,event) action="saveData.html" method=POST>';for (var x=1;x< 9;x++){n += 'Zone '+x+':<input type=text name="zone'+x+'" value="'+zones[x - 1]+'"><br>';}n += '<input value=Save type=submit></form>';zoneNamesForm.innerHTML=n;}</script>)EOF";

const char schedulePage[] PROGMEM = R"EOF(<meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" type="text/css" href="stylesheet.css"><script src="Data.js"></script><object style=width:100%; data="header.html"></object><html><div id=saving style="position:fixed;top:0;bottom:0;left:0;right:0;text-align:center;font-size:3rem;background:rgba(150,150,150,.5);display:none"><div class=popout> Saving Data Please Wait</div></div><div id=save style=text-align:center><button onclick=save()>Save Changes</button><div style=font-size:.75rem;><br>Click Column Heads to Sort Column<br>Click Day Column to Select Run Days</div></div><div id=ss></div><div class=popout id=dayMenu style=display:none;><div id=dayInputMenu></div><button onmousedown=setup() onclick=dayMenu.style.display="none";>Save</button><button onclick=dayMenu.style.display="none";>Cancel</button></div><div  style=text-align:center><button onclick=addTime()>Add Time</button></div></html><script src="schedule.js"></script>)EOF";

const char scheduleJS[] PROGMEM = R"EOF(modified=0;wdays=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];setup();function qs(s){return Array.from(...Array(document.querySelectorAll(s)));}function save(){modified=0;var names=["days","zone","start","runtime"];for (var pos in schedule){var m="id=schedule&pos="+pos;for (var val=0;val<4;val++){m+="&"+names[val]+"="+schedule[pos][val];}var r=new XMLHttpRequest();r.open("POST","saveData.html",true);r.setRequestHeader('Content-type','application/x-www-form-urlencoded');r.send(m);}var d=new XMLHttpRequest();d.open("POST","saveData.html",true);d.setRequestHeader('Content-type','application/x-www-form-urlencoded');d.onreadystatechange = function(){if (this.readyState == 4 && this.status == 200){alert(this.responseText);saving.style.display="none";}};d.send("id=scheduleDone");saving.style.display="";}function getMTime(T){hour=parseInt(T/60,10);minutes=T%60;return [hour,minutes]}function getTime(h,m){console.log(arguments);if (m != undefined){return hour*60+m;}else{var ampm="";hour=parseInt(h/60,10);minutes=h%60;if (hour > 11){ampm="PM";}else{ampm="AM";}if (hour%12==0){hour=12;}else{hour=hour%12;}return [hour,minutes,ampm];}}function updateDays(ele,pos){modified=1;if (ele.dataset.value=="1"){ele.style.background="red";ele.style.color="black";ele.innerText=String.fromCharCode(10008);ele.dataset.value=0;}else{ele.dataset.value=1;ele.style.background="green";ele.style.color="white";ele.innerText=String.fromCharCode(10004);}val=parseInt(qs("."+ele.className).map(function(ele){if(ele.dataset.value=="1")return 1;else return 0;}).join(""),2);schedule[pos][0]=val;}function dayInput(days,pos){days=String(days);console.log(days);var v="<table style=width:20vw;font-size:1.5rem;><tr>";for (var x=0;x<7;x++){v+='<td onclick=updateDays(this,'+pos+') class=p'+pos;if (days[x]=="1"){v+=' data-value=1 style="font-size:1.5rem;cursor:pointer;color:white;background:green;border:black solid 1px">&#10004';}else{v+=' data-value=0 style="font-size:1.5rem;cursor:pointer;color:black;background:red;border:black solid 1px">&#10008';}v+="</td><td>"+wdays[x]+"</td></tr>";}v+="</table>";return v;}function zoneInput(val,pos){var t="<select data-value="+pos+" onchange=update(this)>";for (var x=0;x<zones.length;x++){t+="<option value="+x+" ";if (val == x){t+="selected";}t+=">"+zones[x]+"</input>";}t+="</select>";return t;}function remove(x){schedule[x]=[0,0,0,0];setup();}function addTime(){for (var x in schedule){if (schedule[x][0]==0){schedule[x][1]=0;schedule[x][2]=0;schedule[x][3]=0;schedule[x][0]=127;schedule.sort((a,b)=>{if(b[3]==0) return -1});setup();return;}}alert("All slots are occupied! Delete a schedule point to make space!");}function update(ele){modified=1;pos=ele.dataset.value;if (ele.type=="select-one"){schedule[pos][1]=Number(ele.value);}else if(ele.type=="time"){var val=ele.value.split(":");schedule[pos][2]=(Number(val[0])*60)+Number(val[1]);}else if(ele.type=="number"){schedule[pos][3]=Number(ele.value);}}function mysort(ele){var col=ele.cellIndex;var vals=[];var n=0;for (x in schedule){if (schedule[x][0] && schedule[x][3]){vals[n]=[schedule[x][col],n];n++;}}return vals;}function colSort(ele,rev=0){if (ele.className.indexOf("sorted")>-1){rev=1;ele.className=ele.className.replace("sorted","reverse");}else if(ele.className.indexOf("reverse")>-1){rev=0;ele.className=ele.className.replace("reverse","sorted");}else{for (x in qs(".sorted,.reverse")){qs[x].className=qs[x].className.replace("sorted","");qs[x].className=qs[x].className.replace("reverse","");}ele.className+=" sorted";}col=ele.cellIndex;if(rev){schedule.sort((a,b)=>(b[col]-a[col]));}else{schedule.sort((a,b)=>(a[col]-b[col]));}setup();}function setup(){var weekdays="SMTWTFS";var t='<table class=datatable ><tr><th class="sortable title" onclick=colSort(this,1)>Days</th><th class="sortable title" onclick=colSort(this)>Zone</th><th class="sortable title" onclick=colSort(this)>Time</th><th class="sortable title" onclick=colSort(this)>RunTime</th><th class=title>Action</th></tr>';for (var x=0;x<schedule.length;x++){if (!schedule[x][0]){continue;}var days=schedule[x][0].toString(2).padStart(7,'0');var stringDays="";for (d in days){if (days[d]=="1"){stringDays+="<u><b>"+weekdays[d]+"</u></b>";}else{stringDays+=weekdays[d];}}t+='<tr><td selectable=false style=cursor:pointer onmousedown=dayInputMenu.innerHTML=dayInput("'+days+'",'+x+') onclick=dayMenu.style.display="">'+stringDays+'</td>\<td>'+zoneInput(schedule[x][1],x)+'</td>\<td><input data-value='+x+' onchange=update(this) type=time value='+String(getMTime(schedule[x][2])[0]).padStart(2,"0")+':'+String(getMTime(schedule[x][2])[1]).padStart(2,"0")+'></td>\<td><input data-value='+x+' onchange=update(this) type=number value='+schedule[x][3]+'></td>\<td>\<svg data-value='+x+' onclick=remove('+x+') height="1.5rem" viewBox="0 0 24 24" width="1.5rem" onclick="remove(this,this.parentElement.parentElement.id)">\<path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"></path>\<path d="M0 0h24v24H0z" fill="none"></path>\</svg></td>\</tr>';}t+="</table>";ss.innerHTML=t;})EOF";

const char  indexPage[] PROGMEM = R"EOF(<meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" type="text/css" href="stylesheet.css"><script src="Data.js"></script><object style=width:100%; data="header.html"></object><html><div id=quickrun class=popout style=display:none;text-align:center;><h1>Quick Run</h1> Zone:<select id=qrZone></select><br> Runtime:<input type=number id=qrTime min=0 max=120 style=width:4p></input><br><button onclick=qr(qrZone.value,qrTime.value);quickrun.style.display="none";>Set</button><button onclick=quickrun.style.display="none">Cancel</button><br><button onclick=qr(0,0);quickrun.style.display="none";>Stop</button></div><div id=raindelay class=popout style=display:none;text-align:center;><h1>Rain Delay</h1> Hours:<input type=number id=delay><br><button onclick=qr(8,delay.value*60),this.parentElement.style.display="none">Set</button><button onclick=this.parentElement.style.display="none">Cancel</button></div><div id="info" style="text-align:center"><table style="margin:auto"><tr><td>Current Time:</td><td id="time"></td></tr><tr><td>Active Zones:</td><td id="activeZone"></td></tr><tr><td></td><td></td></tr><tr><td colspan="2"><button onclick="quickrun.style.display=&quot;&quot;">QuickRun</button><button onclick="raindelay.style.display=&quot;&quot;">Rain Delay</button></td></tr></table></div><div id=tableZone></div><object style=width:100%;position:fixed;bottom:0; data="footer.html"></object></html><script src="index.js"></script>)EOF";

const char indexJS[] PROGMEM = R"EOF(wdays=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];function update(){clock = new Date();var now = (clock.getHours() * 60) + (clock.getMinutes());time.innerText = clock.toLocaleTimeString();setTimeout(update,1000);}clock = new Date();update();tableZone.innerHTML=tableMaker();quickRun = [0,0];var zoneStatus = new EventSource("status.html");zoneStatus.addEventListener('message',function(e){var data=JSON.parse(e.data);if (data["Status"] == 255){activeZone.innerText = "None";}else{var endTime=new Date(data["activeTime"]*1000);activeZone.innerText = zones[data["Status"]]+" is Active Until "+endTime.toLocaleString('en-US', { hour: 'numeric', minute: 'numeric', hour12: true });}});function qs(s){return Array.from(...Array(document.querySelectorAll(s)));}function getTime(h,m){if (m){return hour*60+m;}else{var ampm="";var hour=parseInt(h/60,10);var minutes=h%60;if (hour > 11){ampm="PM";}else{ampm="AM";}if (hour%12==0){hour=12;}else{hour=hour%12;}return [hour,minutes,ampm];}}function updateDays(ele,pos){val=parseInt(qs("."+ele.className).map(function(ele){if(ele.checked)return 1;else return 0}).join(""),2);schedule[pos][0]=val;}function dayInput(days,pos){days=String(days);console.log(days);var v="<table><tr>";for (var x=0;x<7;x++){v+='<td><input onchange=updateDays(this,'+pos+') class=p'+pos+' type=checkbox ';if (days[x]=="1"){v+=" checked";}v+=" ></td><td>"+wdays[x]+"</td></tr>";}v+="</table>";return v;}function zoneInput(val,pos){var t="<select data-value="+pos+" onchange=update(this)>";for (var x=0;x<zones.length;x++){t+="<option value="+x+" ";if (val == x){t+="selected";}t+=">"+zones[x]+"</input>";}t+="</select>";return t;}function remove(x){schedule[x]=[0,0,0,0];setup();}function addTime(){for (x in schedule){if (schedule[x][0]==0){schedule[x][0]=127;setup();return;}}alert("All slots are occupied! Delete a schedule point to make space!");}function mysort(ele){var col=ele.cellIndex;var vals=[];var n=0;for (x in schedule){if (schedule[x][0] && schedule[x][3]){vals[n]=[schedule[x][col],n];n++;}}return vals;}function hideRows(cl,visibility){if (visibility == "false"){qs("."+cl).map(ele=>ele.style.display="");qs("#"+cl)[0].dataset.vis="true";}else{qs("."+cl).map(ele=>ele.style.display="none");qs("#"+cl)[0].dataset.vis="false";}}function tableMaker(){var qrz="";for (var x in zones){qrz+="<option value="+x+">"+zones[x]+"</option>";}qrZone.innerHTML=qrz;tabledays=[[],[],[],[],[],[],[]];var page="<table class=datatable><tr><th class=title>Zone</th><th class=title>Start</th><th class=title>Run Time</th></tr>";for (x in schedule){if (schedule[x][3]){days=schedule[x][0].toString(2).padStart(7,"0");for (day in days){if (Number(days[day])){tabledays[day].push([schedule[x][1],schedule[x][2],schedule[x][3]]);}}}}for (day in tabledays){tabledays[day].sort(function(a,b){return a[1]-b[1]});page+='<tr><th class=header colspan=3 onclick=hideRows(this.id,this.dataset.vis) data-vis="false" id='+wdays[day]+'>'+wdays[day]+'</th></tr>';for (spot in tabledays[day]){var startTime=getTime(tabledays[day][spot][1]);page+='<tr class='+wdays[day]+' style=display:none;><td>'+zones[tabledays[day][spot][0]]+'</td><td>'+String(startTime[0]).padStart(2,"0")+':'+String(startTime[1]).padStart(2,"0")+' '+startTime[2]+'</td><td>'+tabledays[day][spot][2]+'</td></tr>';}}return page+="</table>";}function qr(zone,time){var now = Number((clock.getHours() * 60) + clock.getMinutes());quickRun = [zone,time];qs(".popup").forEach(ele=>ele.style.display = "none");var packet = "id=quickRun&zone=" + zone + "&runTime=" + time;var request = new XMLHttpRequest();request.open("POST","saveData.html",true);request.setRequestHeader('Content-type','application/x-www-form-urlencoded');request.onreadystatechange = function(){if (this.readyState == 4 && this.status == 200){console.log(this.responseText);}};request.send(packet);})EOF";

const char header[] PROGMEM = R"EOF(<html><table style=width:100%><tr><td style="text-align:left;width:20%;padding:1rem;"><svg onclick=window.parent.location.href="settings.html"  viewBox="0 0 24 24" width="3rem"><path d="M0 0h24v24H0V0z" fill="none"></path><path d="M19.43 12.98c.04-.32.07-.64.07-.98s-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.07.98s.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z"></path></svg><td><td  style="text-align:center;width:60%;padding:1rem;font-size:2.5rem;font-weight:bold;cursor:pointer" onclick=window.parent.location.href="index.html">Sprinklers</td><td  style="text-align:right;width:20%;padding:1rem;" ><svg onclick=window.parent.location.href="scheduleSetup.html" width="3rem" viewBox="0 0 24 24"><path d="M0 0h24v24H0V0z" fill="none"></path><path d="M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm-.22-13h-.06c-.4 0-.72.32-.72.72v4.72c0 .35.18.68.49.86l4.15 2.49c.34.2.78.1.98-.24.21-.34.1-.79-.25-.99l-3.87-2.3V7.72c0-.4-.32-.72-.72-.72z"></path></svg></td></tr></table></html>)EOF";

const char indexPage_Time[] PROGMEM = R"EOF(<meta name="viewport" content="width=device-width,initial-scale=1"><link rel="stylesheet" type="text/css" href="stylesheet.css"><script src="Data.js"></script><object style=width:100%; data="header.html"></object><html><div id=quickrun class=popout style=display:none;text-align:center;><h1>Quick Run</h1> Zone:<select id=qrZone></select><br> Runtime:<input type=number id=qrTime min=0 max=120 style=width:4p></input><br><button onclick=qr(qrZone.value,qrTime.value);quickrun.style.display="none";>Set</button><button onclick=quickrun.style.display="none">Cancel</button><br><button onclick=qr(0,0);quickrun.style.display="none";>Stop</button></div><div id=raindelay class=popout style=display:none;text-align:center;><h1>Rain Delay</h1> Hours:<input type=number id=delay><br><button onclick=qr(8,delay.value*60),this.parentElement.style.display="none">Set</button><button onclick=this.parentElement.style.display="none">Cancel</button></div><div id="info" style="text-align:center"><table style="margin:auto"><tr><td>Current Time:</td><td id="time"></td></tr><tr><td>Active Zones:</td><td id="activeZone"></td></tr><tr><td></td><td></td></tr><tr><td colspan="2"><button onclick="quickrun.style.display=&quot;&quot;">QuickRun</button><button onclick="raindelay.style.display=&quot;&quot;">Rain Delay</button></td></tr></table></div><div id=tableZone></div><object style=width:100%;position:fixed;bottom:0; data="footer.html"></object></html><script src="index.js"></script><script> function getTime(){ var d=new Date; var tz=d.getTimezoneOffset()/-60; var UTC=d.getTime()/1000; return "UTC="+String(UTC)+"&tz="+String(tz); } function sendTime(){ req=new XMLHttpRequest(); req.open("POST","setTime.html",true); req.setRequestHeader('Content-type', 'application/x-www-form-urlencoded'); req.send(getTime()); } sendTime(); </script>)EOF";

bool wifiCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
    checkWifi = now() + 600;
  }
  return true;
}

bool wifiConnect(String SSID = "", String PW = "") {
  Serial.println("SSID=" + SSID);
  if (SSID == "") {
    Serial.println("Trying Old:");
    Serial.println(data.networkSSID);
    Serial.println(WiFi.begin(data.networkSSID, data.networkPW));
  } else {
    Serial.println("Trying New:");
    WiFi.begin(SSID, PW);
  }
  byte n = 0;
  Serial.print("Connecting ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    n++;
    if (n > 20) {
      return false;
    }
  }
  if (! (SSID == "" )) {
    Serial.println("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\nWriting New Network Credentials:");
    Serial.println(SSID);
    Serial.println(PW);
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    SSID.toCharArray(data.networkSSID, 50);
    PW.toCharArray(data.networkPW, 50);
    EEPROM_Save();
    update_time();
    Serial.print("Setting Network to ");
    Serial.println(data.networkSSID);
    return true;
  }
  else {
    return true;
  }
}

String footer() {
  String conStat = "Offline";
  if (WiFi.status() == WL_CONNECTED) {
    conStat = WiFi.SSID();
  }
  return "<div id=footer style=\"font-size:.75rem; position:fixed;background:lightgray;left:0;right:0;bottom:0px;text-align:center;cursor:pointer\" onclick=top.window.location.href=\"/setup.html\"><u>Connection Status:</u>" + conStat + " LocalIP:" + WiFi.localIP().toString() + "  " + Version + "</div>";
}

String networkSignin() {
  int n = WiFi.scanNetworks();
  String networks = "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" user-scalable=no><html><h2>Networks Found</h2><table><tr><th>SSID</th><th>Signal Quality</th></tr>";
  for (int x = 0; x < n; x++) {
    networks += "<tr><td class=network id=\"" + WiFi.SSID(x) + "\" onclick=setNetwork(this)>" + WiFi.SSID(x) + "</td><td><h3>" + (2 * (WiFi.RSSI(x) + 100)) + "%</h3></td></tr>";
  }
  networks += "</table><h2>Network Information</h2> <form method=POST action=connect.html> <input name=ssid id=ssid placeholder=SSID type=text> <input name=pw id=pw placeholder=password type=password> <input type=submit> </form> </div> </html> <style> .network{ cursor:pointer; padding-bottom:30px; color:blue; text-decoration: underline; font-weight:bold; font-size:200%; } .network:hover{ font-weight:bold; color:purple; } </style> <script> function setNetwork(ele){ ssid.value=ele.id; pw.focus(); } </script>";
  return networks;
}

char Location[50];
String debug = "";
String publicIP;
int  TZoffset, dayTemp, dayMax, dayAvg, dayMin;
float Rain, Frain;
time_t serverCheckin = 0;
//WiFiClientSecure piServer;
WiFiClient piServer;


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
    Serial.print("SSID:");
    Serial.println(data.networkSSID);
  } else {
    Serial.println("EEPROM size changed - EEPROM data zeroed - commit() to make permanent");
  }
}

void fullReset() {
  WiFi.disconnect();
  for (uint8_t z = 0; z < 64; z++) {
    data.schedule[z].days = 0;
    data.schedule[z].zone = 0;
    data.schedule[z].runTime = 0;
    data.schedule[z].start = 0;
  }
  data.smartDelay = 0;
  data.serverUName[0] = 0;
  data.serverPW[0] = 0;
  String("Wifi_Sprinklers").toCharArray(data.ssid, 25);
  data.pagePassword[0] = 0;
  for (uint8_t z = 0; z < 8; z++) {
    ("zone" + String(z)).toCharArray(data.zoneNames[z], 25);
  }
  data.networkSSID[0] = 0;
  data.networkPW[0] = 0;
  data.passwordTimeout = 0;
  data.hidden = false;
  Serial.println("Full Reset Done");
  EEPROM_Save();
  delay(1000);
  ESP.reset();
}

String debugPrint(String message) {
  Serial.print(message);
}

String debugPrintln(String message) {
  Serial.println(message);

}
char * get_JSON(char * buff) {

  snprintf(buff, 5000, "ServerUNameVal=\"%s\"\nLocationVal=\"%s\";\nRainVal=%0.2f;\nFrainVal=%0.2f;\nsmartRainVal=%0.2f;\nZipCodeVal=%d;\nschedule=[", data.serverUName, Location, Rain, Frain, data.smartDelay, data.ZipCode);
  for (uint8_t z = 0; z < 64; z++) {
    if (z == 0) strcat(buff, "[");
    else strcat(buff, ",[");
    snprintf(buff + strlen(buff), 5000, "%d,%d,%d,%d", data.schedule[z].days, data.schedule[z].zone, data.schedule[z].start, data.schedule[z].runTime);
    strcat(buff, "]");
  }
  snprintf(buff + strlen(buff), 5000, "];\nzones=[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"Rain Delay\"];\nSSID=\"%s\";", data.zoneNames[0], data.zoneNames[1], data.zoneNames[2], data.zoneNames[3], data.zoneNames[4], data.zoneNames[5], data.zoneNames[6], data.zoneNames[7], data.ssid);
  Serial.print("Json Data len:");
  Serial.println(strlen(buff));
  return buff;
}



String EEPROM_Save() {
  Serial.println("EEPROM Sector usage:" + String(EEPROM.percentUsed()) + "%");
  EEPROM.put(0, data);
  boolean ok = EEPROM.commit();
  Serial.println((ok) ? "Commit OK" : "Commit Failed");
  return (ok) ? "Commit OK" : "Commit Failed";
}


String software_update(String ip_address) {
  Serial.println("\nStarting update");
  Serial.println(String(ip_address));
  WiFiClient updateClient;
  t_httpUpdate_return ret = ESPhttpUpdate.update(updateClient, ip_address.c_str(), 80, "/Consumer_sprinkler2_0.bin");
  Serial.println("UPDATE VALUE");
  Serial.println(ret);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      return "HTTP_UPDATE_FAILD Error" + String(ESPhttpUpdate.getLastError()) + ":" + String(ESPhttpUpdate.getLastErrorString());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      return "HTTP_UPDATE_NO_UPDATES";
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      return "HTTP_UPDATE_OK";
      break;
  }
}



int getRain(String tag = "none") {
  if (! wifiCheck() ) {
    Serial.println("offline, skipping");
    return 0;
  }
  WiFiClient client;
  client.connect("jeffshomesite.com", 80);
  String cmd = "GET /getData.php";
  cmd += "?tag=" + tag;
  cmd += "&Requester=" + String(WiFi.macAddress());
  Serial.println(data.ZipCode);
  if (data.ZipCode) {
    Serial.println("Adding ZipCode Request");
    cmd += "&data=" + String(data.ZipCode);
  }
  cmd += " HTTP/1.1\r\nHost:jeffshomesite.com\r\n\r\n";
  Serial.println("Get Rain:" + cmd);
  client.print(cmd);
  delay(500);
  Serial.println("Get Rain Return:");
  Serial.println(client.readStringUntil(' '));
  int httpCode = client.parseInt();
  Serial.printf("HTTPCODE: \"%d\"", httpCode);
  if (httpCode != 200 ) {
    return httpCode;
  }
  Serial.println(client.readStringUntil('|'));
  TZoffset = client.parseInt();
  Rain = client.parseFloat();
  Frain = client.parseFloat();
  int mlen = client.readBytesUntil(char(5), Location, 50);
  Location[mlen] = 0;
  dayTemp = client.parseInt();
  dayMax = client.parseInt();
  dayAvg = client.parseInt();
  dayMin = client.parseInt();
  time_t t = client.parseInt();
  setTime(t);
  client.stop();
  if (data.smartDelay && (Rain > data.smartDelay || Frain > data.smartDelay)) {
    quickRun.zone = 8;
    quickRun.runTime = now() + (8 * 3600);
  }
  Serial.print("TZ:");
  Serial.println(TZoffset);
  Serial.print("Rain:");
  Serial.println(Rain);
  Serial.print("Forecast:");
  Serial.println(Frain);
  Serial.print("Time:");
  Serial.println(t);
  Serial.print("Locale Time:");
  Serial.println(lnow());
  Serial.printf("Date: %02d/%02d/%02d\nUTC: %02d:%02d:%02d\nLocal Time: %02d:%02d:%02d\n\n", month(lnow()), day(lnow()), year(lnow()) , hour(), minute(), second(), hour(lnow()), minute(), second());
  //weatherUpdate=now()+(24*3600);
  return httpCode;
}

time_t lnow() {
  return now() + (TZoffset * 3600);
}

void update_time() {
  if (! wifiCheck() ) {
    return ;
  }
  for (uint8_t x = 0; x < 5; x++) {
    int RET = getRain("DailyUpdate");
    if (RET == 200) break;
    delay(100);
  }
  //Serial.println("Setting Time");
  //configTime(TZoffset * 3600, 0, "pool.ntp.org", "time.nist.gov");
  //time_t t = time(NULL);
  //setTime(t);
  time_update = now() + (24 * 3600);
}

void shutoff() {
  //Serial.println("Shutting down Zones");
  for (uint8_t x = 0; x < 8; x++) {
    digitalWrite(pins[x], HIGH);
    activeTime = 0;
    Status = 255;
  }
}

int rightNow() {
  return (hour(now() + (TZoffset * 3600)) * 60) + minute();
}


bool dayCheck(uint8_t n, uint8_t d) {
  bool days[7];
  for (int8_t i = 6; i > -1 ; --i)
  {
    days[i] = n & 1;
    n /= 2;
  }
  return days[d];
}


void runCheck() {
  if (timeStatus() == 0 ) {
    shutoff();
    return;
  }
  if (quickRun.runTime > now()) { //QuickRun always has priority
    if (Status != quickRun.zone) {
      shutoff();
    }
    Status = quickRun.zone;
    digitalWrite(pins[quickRun.zone], LOW);
    activeTime = quickRun.runTime;
    debug = "<h1>Run Info</h1><br>QuickRun Zone:" + String(quickRun.zone) + " End:" + String(quickRun.runTime) + " Rightnow:" + String(now());
  } else {
    quickRun.runTime = 0;
    for (uint8_t x = 0; x < 64; x++) { //scan through schedule
      if (data.schedule[x].runTime) { //get Run time (minutes since midnigth)
        if (dayCheck(data.schedule[x].days, weekday() - 1)) { //check binary day list to see if today is it
          int startTime = data.schedule[x].start;
          int endTime = (startTime + data.schedule[x].runTime);
          int currentTime = rightNow();
          if (currentTime >= startTime && currentTime < endTime) {
            if (Status != data.schedule[x].zone) {
              shutoff();
            }
            digitalWrite(pins[data.schedule[x].zone], LOW);
            activeTime = ((endTime - rightNow()) * 60) + now();
            Status = data.schedule[x].zone;
            debug = "Scheduled Slot:" + String(x) + "Zone:" + String(data.schedule[x].zone) + " RightNow:" + String(currentTime) + " StartTime:" + String(data.schedule[x].start) + " RunTime:" + String(data.schedule[x].runTime);
            return;
          }
        }
      }
    }
  }
  if (activeTime < now() && Status != 255) {
    shutoff();
    debug = "";
  }
}


void serverReconnect() {
  if (data.serverUName[0] != 0 &&  data.serverPW[0] != 0) {
    piServer.stop();
    connect();
  }
}



void setup() {
  Serial.begin(115200);
  Serial.println("\n----------------------------------------------------------------------------------------------------------------------------");
  // Wait for connection
  WiFi.hostname("wifi sprinklers");

  //WiFi.mode(WIFI_STA);
  WiFi.mode(WIFI_AP_STA);
  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(data.ssid);
  delay(500);
  WiFi.printDiag(Serial);
  Serial.println(WiFi.localIP());
  //  WiFiManager wifiManager;
  //  wifiManager.autoConnect("Wifi-Sprinkler-setup");
  EEPROM_Startup();
  Serial.printf("SSID: %s, PW: %s\n", data.networkSSID, data.networkPW);
  wifiConnect();
  pinMode(D8, INPUT);
  Serial.println("\n Booting Up");
  Serial.println("Setting up LittleFS:");
  if (!LittleFS.begin ()) {
    Serial.println ("An Error has occurred while mounting LittleFS");
    return;
  }
  Dir dir = LittleFS.openDir ("");
  Serial.println("LittleFS File Structure:");
  while (dir.next ()) {
    Serial.println (dir.fileName ());
    Serial.println (dir.fileSize ());
  }
  if (wifiCheck()) {
    Serial.println("connected!");
    update_time();
    Serial.println("Time set");
  }
  for (int p = 0; p < 8; p++)
  {
    pinMode(pins[p], OUTPUT);
    digitalWrite(pins[p], HIGH);
  }
  int n;

  Serial.println("Wifi Connected?" + String(wifiCheck()));

  //////////////WEBServer/////////////////


  server.on("/setTime.html", []() {
    time_t  t = server.arg("UTC").toInt();
    TZoffset = server.arg("tz").toInt();
    setTime(t);
    Serial.printf("\n\nUTC: %02d:%02d:%02d \nLocal: %02d:%02d:%02d\n", hour(), minute(), second(), hour() + TZoffset, minute(), second());
    Serial.println("\n\nTime Set: " + String(timeStatus()));
  });
  server.on("/setup.html", []() {
    server.send(200, "text/html", networkSignin() + footer());
  });
  server.on("/connect.html", []() {
    if (wifiConnect(server.arg("ssid"), server.arg("pw"))) {
      server.send(200, "text/html", "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><div style=\"font-size:200%\">Successfully connected to Network " + server.arg("ssid") + " At " + WiFi.localIP().toString() + "</div><div style=text-align=center;margin:2rem;><button style=font-size:150%; onclick=location=\"/index.html\">Go Home</button></div><script>function home(){top.window.location.href=setTimeout(function(){location=\"/index.html\"},3000);" );
    } else {
      server.send(200, "text/html", "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><div style=\"font-size:200%\">Couldn't log in, go back and try again!</div><button style=font-size:150%; onclick=location=\"/index.html\">Go Home</button></div>");
    }
  });


  server.on("/saveData.html", [] () {
    Serial.println("requested saveData.html");
    Serial.println("Request Arguments");
    for (uint8_t x = 0; x < server.args(); x++) {
      Serial.println(server.argName(x) + " = " + server.arg(x));
    }
    if (server.arg("id") == "reboot") {
      server.send(200, "text/html", "Rebooting Device");
      ESP.reset();
    }
    else if (server.arg("id") == "smartFeature") {
      data.ZipCode = server.arg("zipcode").toInt();
      data.smartDelay = server.arg("smartRain").toFloat();
      server.send(200, "text/html", EEPROM_Save());
      getRain("SmartFeatureRequest");
    }
    else if (server.arg("id") == "schedule") {
      uint8_t pos = server.arg("pos").toInt();
      uint8_t z = server.arg("zone").toInt();
      uint8_t d = server.arg("days").toInt();
      int16_t s = server.arg("start").toInt();
      uint8_t r = server.arg("runtime").toInt();
      data.schedule[pos].zone = z;
      data.schedule[pos].days = d;
      data.schedule[pos].start = s;
      data.schedule[pos].runTime = r;
      server.send(200, "text/html", "recieved");
    }
    else if (server.arg("id") == "AP") {
      server.arg("ssid").toCharArray(data.ssid, 25);
      if (server.arg("hidden").toInt()) {
        data.hidden = true;
      } else {
        data.hidden = false;
      }
      server.send(200, "text/html", EEPROM_Save());
      WiFi.softAP(data.ssid, "", 1, data.hidden, 4);
    }
    else if (server.arg("id") == "scheduleDone") {
      server.send(200, "text/html", EEPROM_Save());
      Serial.println(get_JSON(bigbuff));
    }
    else if (server.arg("id") == "zoneNames") {
      server.arg("zone1").toCharArray(data.zoneNames[0], 25);
      server.arg("zone2").toCharArray(data.zoneNames[1], 25);
      server.arg("zone3").toCharArray(data.zoneNames[2], 25);
      server.arg("zone4").toCharArray(data.zoneNames[3], 25);
      server.arg("zone5").toCharArray(data.zoneNames[4], 25);
      server.arg("zone6").toCharArray(data.zoneNames[5], 25);
      server.arg("zone7").toCharArray(data.zoneNames[6], 25);
      server.arg("zone8").toCharArray(data.zoneNames[7], 25);
      server.send(200, "text/html", EEPROM_Save());
    }
    else if (server.arg("id") == "Update") {
      //server.send(200, "text/html", "Device is updating...");
      server.send(200, "text/html", software_update(String(server.arg("source"))));
    }
    else if (server.arg("id") == "reset") {
      server.send(200, "text/html", "Wiping memory, device will reset now");
      fullReset();
    }
    else if (server.arg("id") == "remoteServer") {
      server.arg("serverUName").toCharArray(data.serverUName, 25);
      server.arg("serverPW").toCharArray(data.serverPW, 25);
      piServer.stop();
      if (connect()) {
        delay(10);
        server.send(200, "text/html", EEPROM_Save());
        Serial.println("ServerUName:" + String(data.serverUName) + ",ServerPW:" + String(data.serverPW));
      } else {
        server.send(200, "text/html", "Remote Server:" + String(ServerMessage));
        data.serverUName[0] = 0;
        data.serverPW[0] = 0;
      }
    }
    else if (server.arg("id") == "setPassword") {
      server.arg("pw1").toCharArray(data.pagePassword, 25);
      data.passwordTimeout = server.arg("timeout").toInt();
      server.send(200, "text/html", EEPROM_Save());
      Serial.println("Password Set To:" + String(data.pagePassword));
      delay(10);
    }
    else if (server.arg("id") == "rainDelay") {
      quickRun.runTime = server.arg("hours").toInt() * 3600 + now();
      quickRun.zone = 8;
    }
    else if (server.arg("id") == "quickRun") {
      quickRun.zone = server.arg("zone").toInt();
      if (server.arg("runTime") == "0") {
        quickRun.runTime = 0;
        shutoff();
      } else {
        quickRun.runTime = server.arg("runTime").toInt() * 60 + now();
      }
      Serial.println("Right Now=" + String(now()) + ", Setting zone " + String(quickRun.zone) + " until " + String(quickRun.runTime));
      server.send(200, "text/html", "QuickRun Started");
      delay(10);
    }
    if (server.arg("id") == "login") {
      Serial.println(server.arg("pw") + " vs " + data.pagePassword);
      if (server.arg("pw") == String(data.pagePassword)) {
        Serial.println("Correct Password");
        lastLogin = now() + (data.passwordTimeout * 60);
        server.send(200, "text/html",  "<script>location=\"/index.html\"</script>");
      } else {
        Serial.println("Wrong Password");
        server.send_P(200, "text/html", loginPage);
      }
    }
  });
  server.on("/header.html", [] () {
    server.send_P(200, "text/html", header);
  });
  server.on("/Data.js", [] () {
    server.send(200, "application/javascript", get_JSON(bigbuff));
  });
  server.on("/footer.html", [] () {
    server.send(200, "text/html", footer());
    //server.send(200, "text/html", "<div style=\"left:0px;position:fixed;bottom:0px;font-size:50%;text-align:center;background:white;right:0px;\">Local IP:" + WiFi.localIP().toString() + " " + Version + "</div>");
  });
  server.on("/stylesheet.css", [] () {
    server.send_P(200, "text/css", styleSheet);
    delay(100);
  });
  server.on("/schedule.js", []() {
    server.send_P(200, "application/javascript", scheduleJS);
    delay(100);
  });
  server.on("/index.js", []() {
    server.send_P(200, "application/javascript", indexJS);
    delay(100);
  });
  server.on("/status.html", []() {
    server.send(200, "text/event-stream", "retry:1000\ndata:{\"Status\":" + String(Status) + ",\"activeTime\":" + String(activeTime) + "}\n\n");
  });
  server.on("/", []() {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send_P(200, "text/html", loginPage);
      delay(100);
    } else {
      server.send_P(200, "text/html", timeStatus() == 2 ? indexPage : indexPage_Time);
      delay(100);
    }
  });
  server.on("/index.html", []() {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send_P(200, "text/html", loginPage);
      delay(100);;
    } else {
      server.send_P(200, "text/html", timeStatus() == 2 ? indexPage : indexPage_Time);
      delay(100);
      //server.send_P(200, "text/html", indexPage);
      //delay(100);
    }
  });
  server.on("/scheduleSetup.html", [] () {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send_P(200, "text/html", loginPage);
    } else {
      server.send_P(200, "text/html", schedulePage);
      delay(100);
    }
  });
  server.onNotFound([]() {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send_P(200, "text/html", loginPage);
    } else {
      Serial.print("Time Status:");
      Serial.println(timeStatus());
      server.send_P(200, "text/html", timeStatus() == 2 ? indexPage : indexPage_Time);
      //server.send_P(200, "text/html", indexPage);
      Serial.print("Time Status:");
      Serial.println(timeStatus());
      delay(100);
    }
  });
  server.on("/settings.html", []() {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send_P(200, "text/html", loginPage);
    } else {
      server.send_P(200, "text/html", settingsPage);

    }
  });
  server.on("/debug.html", []() {
    server.send(200, "text/html", debug + "<h1>Info</h1><br>Uptime" + String(millis() / 1000) + "<br><br><h1>Server Info</h1><br>TimeSince Last Message:" + String(now() - lastMessage) + "<br> Pi Server Connected:" + String(piServer.connected()) + "<br>serverCheckin < now?:" + String(serverCheckin < now()) + ",Difference:" + String(serverCheckin - now()) + "<br>ServerUName Defined: " + String(data.serverUName[0] != 0) + "<br>Server Uname:" + String(data.serverUName));
  });
  server.on("/favicon.ico", []() {
    File file = LittleFS.open("/favicon.ico", "r");
    size_t sent = server.streamFile(file, "image/png");
    file.close();
  });
  server.on("/favicon.png", []() {
    Serial.println("Requested /favicon.png");
    String str = "";
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
      str += dir.fileName();
      str += " / ";
      str += dir.fileSize();
      str += "\r\n";
    }
    Serial.print(str);
    File file = LittleFS.open("/favicon.png", "r");
    size_t sent = server.streamFile(file, "image/png");
    Serial.print("Sent:" + String(sent));
    file.close();
    delay(1000);
  });
  server.begin();
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("HTTP server started");
}


void loop() {
  if (digitalRead(D8) == HIGH) {
    Serial.println("Checking for hard reset, waiting 5 seconds");
    for (uint8_t x = 0; x < 5; x++) {
      delay(1000);
      if (digitalRead(D8) == LOW) {
        ESP.reset();
      }
    }
    if (digitalRead(D8) == HIGH) {
      fullReset();
    }
  }
  if (now() > time_update) { // || year() < 2020) {
    update_time();
    serverReconnect();
  }
  yield();
  dnsServer.processNextRequest();
  server.handleClient();
  runCheck();
  if (serverCheckin < now() && data.serverUName[0] != 0) {
    if (now() - lastMessage > 90) {
      Serial.println("Server hasn't responded in over a minute! Stopping");
      serverReconnect();
      lastMessage = now();
    }
    if (!piServer.connected()) {
      connect();
    }
    delay(500);
    while (piServer.available()) {
      Serial.println("Getting data from Server");
      serverRecv();
    }
    runCheck();
    piServer.print(dataDump(bigbuff));
    piServer.print(char(3));
    serverCheckin = now() + 3;
  }
  delay(1);
}
