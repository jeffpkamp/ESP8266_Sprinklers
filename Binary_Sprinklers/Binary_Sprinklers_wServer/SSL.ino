//
//const char trustRoot[] PROGMEM  = R"EOF(
//-----BEGIN CERTIFICATE-----
//MIICkzCCAfygAwIBAgIJAK6eXPWzUZIFMA0GCSqGSIb3DQEBCwUAMGExCzAJBgNV
//BAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBX
//aWRnaXRzIFB0eSBMdGQxGjAYBgNVBAMMEWplZmZzaG9tZXNpdGUuY29tMB4XDTIw
//MTExMTA0MjIxMloXDTI5MDEyODA0MjIxMlowYTELMAkGA1UEBhMCQVUxEzARBgNV
//BAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMgUHR5IEx0
//ZDEaMBgGA1UEAwwRamVmZnNob21lc2l0ZS5jb20wgZ8wDQYJKoZIhvcNAQEBBQAD
//gY0AMIGJAoGBAMOzHa56sSuJQYiAwkIQAr7154NvkVhn7q5ei32AbJwG5v5jmjus
//7drkiHGQuNQ3gEEeOvx48IkWbVLrenWwmBvT2FJKi5yN2E2YhWrjmt/32lmpnRlY
//dV5e8E5Y5YAnPUoY10JS7avdKg67qSuSG1iaav3oxdY/thDUUhRnkcOnAgMBAAGj
//UzBRMB0GA1UdDgQWBBQQcEdL3GXZ/T3IGvyFscvCwP7JZTAfBgNVHSMEGDAWgBQQ
//cEdL3GXZ/T3IGvyFscvCwP7JZTAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEB
//CwUAA4GBAIBCaFdMrsk2620PQvM3/zsdSm1GaBOswnjl+g8BU++PNbOFFBEoFm1L
//HQK93B4p8V+qp/LFCBSubn82TpkLjOqjIoLmlwYwQAiIHlx/12L+dWJ6BoBPKq2/
//6KI3SvDbVE7rRbbUzBZI1gnryLCu6F0wThl0rvKoTAkS8hBYp9fs
//-----END CERTIFICATE-----
//)EOF";
//X509List cert(trustRoot);



char * dataDump(char * buff) {
  strcpy(buff, "{\"schedule\":[");
  //String s = "{\"schedule\":[";
  for (uint8_t z = 0; z < 64; z++) {
    if (z == 0) strcat(buff, "[");
    else strcat(buff, ",[");
    snprintf(buff + strlen(buff), 1500, "%d,%d,%d,%d", data.schedule[z].days, data.schedule[z].zone, data.schedule[z].start, data.schedule[z].runTime);
    strcat(buff, "]");
  }
  snprintf(buff + strlen(buff), 1500, "],\"zones\":[\"%s", data.zoneNames[0]);
  for (uint8_t z = 1; z < 8; z++) {
    snprintf(buff + strlen(buff), 1500, "\",\"%s" , data.zoneNames[z]);
  }
  snprintf(buff + strlen(buff), 1500, "\",\"Rain Delay\"],\"SSID\":\"%s\",\"Status\":%d,\"activeTime\":%jd}", data.ssid, Status, activeTime);
  return buff;
}


bool connect() {
  if (! wifiCheck() ){
    return false;
  }
  int n;
  //  piServer.setTrustAnchors(&cert);
  //  piServer.setX509Time(now());
  if (now() - lastMessage < 60){
    return false;
  }
  if (piServer.connected()) {
    return true;
  } else {
    piServer.connect("jeffshomesite.com", 45002);
    char macaddr[16];
    WiFi.macAddress().toCharArray(macaddr, 16);
    n = snprintf(bigbuff, 1500, "{\"User\":\"%s\",\"password\":\"%s\",\"Device\":\"SprinklerController\",\"Mac\":\"%s\"}", data.serverUName, data.serverPW, macaddr);
    bigbuff[n] = 0;
    piServer.print(bigbuff);
    n = piServer.readBytesUntil(char(5), bigbuff, 1500);
    bigbuff[n] = 0;
    Serial.printf("Server Login Message:%s\n", bigbuff);
    if (piServer.connected() && strcmp(bigbuff, "Success") == 0) {
      lastMessage = now();
      return true;
    } else {
      lastMessage=now();
      //data.serverUName[0] = 0;
      //data.serverPW[0] = 0;
      return false;
    }
  }
}

bool serverRecv() {
  int n;
  piServer.setTimeout(500);
  n = piServer.readBytesUntil('|', bigbuff, 1500);
  bigbuff[n] = 0;
  n = piServer.readBytesUntil(char(5), bigbuff, 1500);
  bigbuff[n] = 0;
  if (bigbuff[0] == 0) return false;
  else {
    lastMessage = now();
    parse(bigbuff);
  }
}

bool parse(char * cmd) {
  Serial.printf("Recieved:>%s<\n", cmd);
  if (strcmp(cmd,"schedule")==0) {
    uint8_t pos = piServer.parseInt();
    data.schedule[pos].days = piServer.parseInt();
    data.schedule[pos].zone = piServer.parseInt();
    data.schedule[pos].start = piServer.parseInt();
    data.schedule[pos].runTime = piServer.parseInt();
  } else if (strcmp(cmd, "ScheduleDone")==0) {
    EEPROM_Save();
  } else if (strcmp(cmd,"quickRun")==0) {
    quickRun.zone = piServer.parseInt();
    int T = piServer.parseInt();
    if (T == 0) {
      quickRun.runTime = 0;
      shutoff();
    } else {
      quickRun.runTime = now() + (T * 60);
    }
    Serial.printf("QuickRun set to Zone %d for %d minutes",quickRun.zone,(quickRun.runTime-now())/60);
  } else if (cmd == "rainDelay") {
    quickRun.zone = 8;
    quickRun.runTime = now() + (piServer.parseInt() * 3600);
  } else if (strcmp(cmd,"update")==0) {
    piServer.print(dataDump(bigbuff));
    piServer.print(char(3));
  } else if (strcmp(cmd,"Checkin")==0){
    lastMessage=now();
  }else{
    Serial.printf("Unknown Requst:%s",cmd);
  }
  return 1;
}
