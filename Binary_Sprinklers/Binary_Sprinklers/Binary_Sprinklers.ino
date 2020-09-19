#include <ESP8266WiFi.h>
#include "DNSServer.h"
#include <ESP8266WebServer.h>
#include <_time.h>
#include <ESP8266httpUpdate.h>
#include <WiFiManager.h>
#include <TimeLib.h>
#include <ESP_EEPROM.h>


//header nonsense
char * settingsPage();
char * schedulePage();
char * indexPage() ;
char * styleSheet();
char * header();


const byte        DNS_PORT = 53;          // Capture DNS requests on port 53
DNSServer         dnsServer;              // Create the DNS object
ESP8266WebServer  server(80);
String Version = " Complied:"+String(__DATE__);
String tPassword = "";
time_t time_update, lastLogin = 0;
char IPADDRESS[16];
byte pins[] = {D0, D1, D2, D3, D4, D5, D6, D7, 60};
int quickRun[2] = {0, 0};
time_t activeTime = 0;
byte Status = 255;
time_t passwordTimeout = 0;

struct schedulePoint {
  byte days;
  byte zone;
  byte runTime;
  int16_t start;
};

struct myData {
  schedulePoint schedule[64];
  char pagePassword[25] = "";
  byte passwordTimeout;
  char ssid[25] = "Wifi_Sprinklers";
  bool hidden = false;
  char zoneNames[8][25] = {"zone1", "zone2", "zone3", "zone4", "zone5", "zone6", "zone7", "zone8"};
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
  for (byte z = 0; z < 64; z++) {
    data.schedule[z].days = 0;
    data.schedule[z].zone = 0;
    data.schedule[z].runTime = 0;
    data.schedule[z].start = 0;
  }
  String("Wifi_Sprinklers").toCharArray(data.ssid, 25);
  String("").toCharArray(data.pagePassword, 25);
  for (byte z = 0; z < 8; z++) {
    ("Zone" + String(z)).toCharArray(data.zoneNames[z], 25);
  }
  data.passwordTimeout = 0;
  data.hidden = false;
  EEPROM_Save();
  delay(1000);
  ESP.reset();
}


String get_JSON() {
  String s = "<script> schedule=[";
  for (byte z = 0; z < 64; z++) {
    if (z == 0) s += "[";
    else s += ",[";
    s += String(data.schedule[z].days) + "," + String(data.schedule[z].zone) + "," + String(data.schedule[z].start) + "," + String(data.schedule[z].runTime);
    s += "]";
  }
  s += "]; zones=[\"" + String(data.zoneNames[0]);
  for (byte z = 1; z < 8; z++) {
    s += "\",\"" + String(data.zoneNames[z]);
  }
  s += "\"];SSID=\"" + String(data.ssid) + "\";</script>";
  return s;
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
  t_httpUpdate_return ret = ESPhttpUpdate.update(ip_address.c_str(), 80, "/Consumer_sprinkler2_0.bin");
  Serial.println("UPDATE VALUE");
  Serial.println(ret);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      return "HTTP_UPDATE_FAILD Error" +String(ESPhttpUpdate.getLastError())+":"+String(ESPhttpUpdate.getLastErrorString());
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


void update_time() {
  configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  delay(5000);
  configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(100);;
  }
  time_t t = time(NULL);
  setTime(t);
  time_update = now() + (24 * 3600);
}
void shutoff() {
  Serial.println("Shutting down Zones");
  for (byte x = 0; x < 8; x++) {
    digitalWrite(pins[x], HIGH);
    activeTime = 0;
    Status = 255;
  }
}

int rightNow() {
  return (hour() * 60) + minute();
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
  if (quickRun[1] > now()) {
    if (Status != quickRun[0]) {
      shutoff();
    }
    Status = quickRun[0];
    digitalWrite(pins[quickRun[0]], LOW);
    activeTime = quickRun[1];
  } else {
    for (byte x = 0; x < 64; x++) {
      if (data.schedule[x].runTime) {
        if (dayCheck(data.schedule[x].days, weekday() - 1)) {
          int startTime = data.schedule[x].start;
          int endTime = (startTime + data.schedule[x].runTime);
          if (rightNow() >= startTime && rightNow() < endTime) {
            if (Status != data.schedule[x].zone) {
              shutoff();
            }
            digitalWrite(pins[data.schedule[x].zone], LOW);
            activeTime = ((endTime - rightNow()) * 60) + now();
            Status = data.schedule[x].zone;
            return;
          }
        }
      }
    }
  }
  if (activeTime < now() && Status != 255) {
    shutoff();
  }
}


char * loginPage() {
  char * page = R"page(<meta name="viewport" content="width=device-width, initial-scale=1" user-scalable=no><style>html{font-size:4vw;}@media screen and (min-width: 700px){html{font-size:35px;}}</style><html><div style=position:absolute;top:25%;left:25%;text-align:center;>Login<form method=post action="saveData.html"><input type=text style=display:none value="login" name=id><input type=password placeholder=password name=pw><input type=submit></form></div></html>
  )page";
  return page;
}

void setup() {
  pinMode(D8, INPUT);
  Serial.begin(115200);
  Serial.println();
  WiFiManager wifiManager;
  wifiManager.autoConnect("Wifi-Sprinkler-setup");
  Serial.println("connected!");
  update_time();
  Serial.println("Time set");
  EEPROM_Startup();
  for (int p = 0; p < 8; p++)
  {
    pinMode(pins[p], OUTPUT);
    digitalWrite(pins[p], HIGH);
  }
  int n;
  // Wait for connection
  WiFi.hostname("wifi sprinklers");
  Serial.println("");
  Serial.print("Connected to ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.mode(WIFI_AP_STA);
  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAP(data.ssid);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  //////////////WEBServer/////////////////

  server.on("/stylesheet.css", [] () {
    server.send(200, "text/css", styleSheet());
  });
  server.on("/saveData.html", [] () {
    Serial.println("requested saveData.html");
    Serial.println("Request Arguments");
    for (byte x = 0; x < server.args(); x++) {
      Serial.println(server.argName(x) + " = " + server.arg(x));
    }
    if (server.arg("id") == "reboot") {
      server.send(200, "text/html", "Rebooting Device");
      ESP.reset();
    }
    if (server.arg("id") == "schedule") {
      byte pos = server.arg("pos").toInt();
      byte z = server.arg("zone").toInt();
      byte d = server.arg("days").toInt();
      int16_t s = server.arg("start").toInt();
      byte r = server.arg("runtime").toInt();
      data.schedule[pos].zone = z;
      data.schedule[pos].days = d;
      data.schedule[pos].start = s;
      data.schedule[pos].runTime = r;
      server.send(200, "text/html", "recieved");
    }
    if (server.arg("id") == "AP") {
      server.arg("ssid").toCharArray(data.ssid, 25);
      if (server.arg("hidden").toInt()) {
        data.hidden = true;
      } else {
        data.hidden = false;
      }
      server.send(200, "text/html", EEPROM_Save());
      WiFi.softAP(data.ssid, "", 1, data.hidden, 4);
    }
    if (server.arg("id") == "scheduleDone") {
      server.send(200, "text/html", EEPROM_Save());
      Serial.println(get_JSON());
    }
    if (server.arg("id") == "zoneNames") {
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
    if (server.arg("id") == "Update") {
      //server.send(200, "text/html", "Device is updating...");
      server.send(200,"text/html", software_update(String(server.arg("source"))));
    }
    if (server.arg("id") == "reset") {
      server.send(200, "text/html", "Wiping memory, device will reset now");
      fullReset();
    }
    if (server.arg("id") == "setPassword") {
      server.arg("value").toCharArray(data.pagePassword, 25);
      data.passwordTimeout = server.arg("timeout").toInt();
      server.send(200, "text/html", EEPROM_Save());
      delay(10);;
    }
    if (server.arg("id") == "rainDelay") {
      quickRun[1] = server.arg("hours").toInt() * 3600 + now();
      quickRun[0] = 8;
    }
    if (server.arg("id") == "quickRun") {
      quickRun[0] = server.arg("zone").toInt();
      if (server.arg("runTime") == "0") {
        quickRun[1] = 0;
        shutoff();
      } else {
        quickRun[1] = server.arg("runTime").toInt() * 60 + now();
      }
      Serial.println("Right Now=" + String(now()) + ", Setting zone " + String(quickRun[0]) + " until " + String(quickRun[1]));
      server.send(200, "text/html", "QuickRun Started");
      delay(10);;
    }
    if (server.arg("id") == "login") {
      if (server.arg("pw") == data.pagePassword) {
        lastLogin = now() + (data.passwordTimeout * 60);
        server.send(200, "text/html",  "<script>location=\"/index.html\"</script>");
      } else {
        server.send(200, "text/html", loginPage());
      }
    }
  });
  server.on("/status.html", []() {
    server.send(200, "text/event-stream", "retry:1000\ndata:{\"Status\":" + String(Status) + ",\"activeTime\":"+String(activeTime)+"}\n\n");
  });
  server.on("/", []() {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send(200, "text/html", loginPage());
      delay(100);;
    } else {
      Serial.println("got / request");
      server.send(200, "text/html", get_JSON() + header() + indexPage() + "<div style=\"left:0px;position:fixed;bottom:0px;font-size:50%;text-align:center;background:white;right:0px;\">Local IP:" + WiFi.localIP().toString() + "</div>");
    }
  });
  server.on("/index.html", []() {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send(200, "text/html", loginPage());
      delay(100);;
    } else {
      Serial.println("got /index.html request");
      server.send(200, "text/html", get_JSON() + header() + indexPage() + "<div style=\"left:0px;position:fixed;bottom:0px;font-size:50%;text-align:center;background:white;right:0px;\">Local IP:" + WiFi.localIP().toString() + " " + Version + "</div>");
    }
  });
  server.on("/scheduleSetup.html", [] () {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send(200, "text/html", loginPage());
    } else {
      Serial.println("requested scheduleSetup.html");
      server.send(200, "text/html", get_JSON() + header() + schedulePage());
    }
  });
  server.onNotFound([]() {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send(200, "text/html", loginPage());
    } else {
      Serial.println("requested something unknown");
      server.send(200, "text/html", get_JSON() + header() + indexPage() + "<div style=\"left:0px;position:fixed;bottom:0px;font-size:50%;text-align:center;background:white;right:0px;\">Local IP:" + WiFi.localIP().toString() + "</div>");
    }
  });
  server.on("/settings.html", []() {
    if (data.passwordTimeout && now() > lastLogin) {
      server.send(200, "text/html", loginPage());
    } else {
      Serial.println("requested settings.html");
      server.send(200, "text/html", get_JSON() + header() + settingsPage());
    }
  });
  server.on("/favicon.ico", []() {
    server.send(200, "text/html", "");
  });
  server.begin();
  dnsServer.start(DNS_PORT, "*", apIP);
  Serial.println("HTTP server started");
}


void loop() {
  if (digitalRead(D8) == HIGH) {
    Serial.println("Checking for hard reset, waiting 5 seconds");
    for (byte x = 0; x < 5; x++) {
      delay(1000);
      Serial.println(digitalRead(D8));
    }
    if (digitalRead(D8) == HIGH) {
      fullReset();
    }
  }
  if (now() > time_update || year() < 2020) {
    update_time();
  }
  yield();
  dnsServer.processNextRequest();
  server.handleClient();
  runCheck();
  if (Serial.available()) {
    char input = Serial.read();
    if (input == 's') {
      Serial.println("Status:" + String(Status) + " Right Now:" + String(now()) + " Active Until:" + String(activeTime));
    }
    if (input == 't') {
      Serial.println(String(month()) + " " + String(day()) + " " + String(hour()) + " " + String(minute()) + " " + String(second()) + " " + String(weekday()));
    }
    if (input == 'd') {
      byte d = Serial.parseInt();
      byte t = Serial.parseInt();
      Serial.println("Daycheck " + String(d) + " " + String(t) + " " + String(dayCheck(d, t)));
    }
  }
}
