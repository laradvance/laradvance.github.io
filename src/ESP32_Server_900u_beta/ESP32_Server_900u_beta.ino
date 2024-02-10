#if !defined(CONFIG_IDF_TARGET_ESP32S2)
#error "Selected board not supported"
#endif

#include <FS.h>
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "esp_task_wdt.h"
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "goldhen.h"
#include "exploit.h"
#include "USB.h"
#include "USBMSC.h"
#include "exfathax.h"
#include "jzip.h"
#define PowerPin 15 //esp s2 mini onboard led
#include "SPIFFS.h"
#define FILESYS SPIFFS

//-------------------DEFAULT SETTINGS------------------//
//create access point
boolean startAP = true;
String AP_SSID = "PS4_HEN";
String AP_PASS = "";
IPAddress Server_IP(10, 1, 1, 1);
IPAddress Subnet_Mask(255, 255, 255, 0);

//connect to wifi
boolean connectWifi = false;
String WIFI_SSID = "Home_WIFI";
String WIFI_PASS = "password";
String WIFI_HOSTNAME = "ps4.local";

//server port
int WEB_PORT = 80;

//Auto Usb Wait(milliseconds)
int USB_WAIT = 5000;

// Displayed firmware version
String firmwareVer = "2.20";

//ESP sleep after x minutes
boolean espSleep = true;
int TIME2SLEEP = 10;  // minutes

//LED Status
boolean ledStatus = true;

//Fan Threshold
boolean fanThres = false;
int TEMPERATURE = 75;  // degree celcius

//-----------------------------------------------------//
#include "Pages.h"
DNSServer dnsServer;
AsyncWebServer server(WEB_PORT);
boolean hasEnabled = true;
boolean powerdown = false;
boolean ledOn = false;
boolean isFormating = false;
long bootTime = 0;
int millisBak;
File upFile;
USBMSC dev;

String split(String str, String from, String to) {
  String tmpstr = str;
  tmpstr.toLowerCase();
  from.toLowerCase();
  to.toLowerCase();
  int pos1 = tmpstr.indexOf(from);
  int pos2 = tmpstr.indexOf(to, pos1 + from.length());
  String retval = str.substring(pos1 + from.length(), pos2);
  return retval;
}

bool instr(String str, String search) {
  int result = str.indexOf(search);
  if (result == -1) {
    return false;
  }
  return true;
}


String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + " B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + " KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + " MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
  }
}


String urlencode(String str) {
  String encodedString = "";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encodedString += '+';
    } else if (isalnum(c)) {
      encodedString += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
    yield();
  }
  encodedString.replace("%2E", ".");
  return encodedString;
}


void sendwebmsg(AsyncWebServerRequest *request, String htmMsg) {
  String tmphtm = "<!DOCTYPE html><html><head><link rel=\"stylesheet\" href=\"style.css\"></head><center><br><br><br><br><br><br>" + htmMsg + "</center></html>";
  request->send(200, "text/html", tmphtm);
}

void handleDelete(AsyncWebServerRequest *request) {
  if (!request->hasParam("file", true)) {
    request->redirect("/fileman.html");
    return;
  }
  String path = request->getParam("file", true)->value();
  if (path.length() == 0) {
    request->redirect("/fileman.html");
    return;
  }
  if (FILESYS.exists("/" + path) && path != "/" && !path.equals("config.ini")) {
    FILESYS.remove("/" + path);
  }
  request->redirect("/fileman.html");
}



void handleFileMan(AsyncWebServerRequest *request) {
  File dir = FILESYS.open("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Manager</title><link rel=\"stylesheet\" href=\"style.css\"><style>body{overflow-y:auto;} th{border: 1px solid #dddddd; background-color:gray;padding: 8px;}</style><script>function statusDel(fname) {var answer = confirm(\"Are you sure you want to delete \" + fname + \" ?\");if (answer) {return true;} else { return false; }} </script></head><body><br><table id=filetable></table><script>var filelist = [";
  int fileCount = 0;
  while (dir) {
    File file = dir.openNextFile();
    if (!file) {
      dir.close();
      break;
    }
    String fname = String(file.name());
    if (fname.length() > 0 && !fname.equals("config.ini") && !file.isDirectory()) {
      fileCount++;
      fname.replace("|", "%7C");
      fname.replace("\"", "%22");
      output += "\"" + fname + "|" + formatBytes(file.size()) + "\",";
    }
    file.close();
    esp_task_wdt_reset();
  }
  if (fileCount == 0) {
    output += "];</script><center>No files found<br>You can upload files using the <a href=\"/upload.html\" target=\"mframe\"><u>File Uploader</u></a> page.</center></p></body></html>";
  } else {
    output += "];var output = \"\";filelist.forEach(function(entry) {var splF = entry.split(\"|\"); output += \"<tr>\";output += \"<td><a href=\\\"\" +  splF[0] + \"\\\">\" + splF[0] + \"</a></td>\"; output += \"<td>\" + splF[1] + \"</td>\";output += \"<td><a href=\\\"/\" + splF[0] + \"\\\" download><button type=\\\"submit\\\">Download</button></a></td>\";output += \"<td><form action=\\\"/delete\\\" method=\\\"post\\\"><button type=\\\"submit\\\" name=\\\"file\\\" value=\\\"\" + splF[0] + \"\\\" onClick=\\\"return statusDel('\" + splF[0] + \"');\\\">Delete</button></form></td>\";output += \"</tr>\";}); document.getElementById(\"filetable\").innerHTML = \"<tr><th colspan='1'><center>File Name</center></th><th colspan='1'><center>File Size</center></th><th colspan='1'><center><a href='/dlall' target='mframe'><button type='submit'>Download All</button></a></center></th><th colspan='1'><center><a href='/format.html' target='mframe'><button type='submit'>Delete All</button></a></center></th></tr>\" + output;</script></body></html>";
  }
  request->send(200, "text/html", output);
}


void handleDlFiles(AsyncWebServerRequest *request) {
  File dir = FILESYS.open("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Downloader</title><link rel=\"stylesheet\" href=\"style.css\"><style>body{overflow-y:auto;}</style><script type=\"text/javascript\" src=\"jzip.js\"></script><script>var filelist = [";
  int fileCount = 0;
  while (dir) {
    File file = dir.openNextFile();
    if (!file) {
      dir.close();
      break;
    }
    String fname = String(file.name());
    if (fname.length() > 0 && !fname.equals("config.ini") && !file.isDirectory()) {
      fileCount++;
      fname.replace("\"", "%22");
      output += "\"" + fname + "\",";
    }
    file.close();
    esp_task_wdt_reset();
  }
  if (fileCount == 0) {
    output += "];</script></head><center>No files found to download<br>You can upload files using the <a href=\"/upload.html\" target=\"mframe\"><u>File Uploader</u></a> page.</center></p></body></html>";
  } else {
    output += "]; async function dlAll(){var zip = new JSZip();for (var i = 0; i < filelist.length; i++) {if (filelist[i] != ''){var xhr = new XMLHttpRequest();xhr.open('GET',filelist[i],false);xhr.overrideMimeType('text/plain; charset=x-user-defined'); xhr.onload = function(e) {if (this.status == 200) {zip.file(filelist[i], this.response,{binary: true});}};xhr.send();document.getElementById('fp').innerHTML = 'Adding: ' + filelist[i];await new Promise(r => setTimeout(r, 50));}}document.getElementById('gen').style.display = 'none';document.getElementById('comp').style.display = 'block';zip.generateAsync({type:'blob'}).then(function(content) {saveAs(content,'esp_files.zip');});}</script></head><body onload='setTimeout(dlAll,100);'><center><br><br><br><br><div id='gen' style='display:block;'><div id='loader'></div><br><br>Generating ZIP<br><p id='fp'></p></div><div id='comp' style='display:none;'><br><br><br><br>Complete<br><br>Downloading: esp_files.zip</div></center></body></html>";
  }
  request->send(200, "text/html", output);
}


void handleConfig(AsyncWebServerRequest *request) {
  if (request->hasParam("ap_ssid", true) && request->hasParam("ap_pass", true) && request->hasParam("web_ip", true) && request->hasParam("web_port", true) && request->hasParam("subnet", true) && request->hasParam("wifi_ssid", true) && request->hasParam("wifi_pass", true) && request->hasParam("wifi_host", true) && request->hasParam("usbwait", true)) {
    AP_SSID = request->getParam("ap_ssid", true)->value();
    if (!request->getParam("ap_pass", true)->value().equals("********")) {
      AP_PASS = request->getParam("ap_pass", true)->value();
    }
    WIFI_SSID = request->getParam("wifi_ssid", true)->value();
    if (!request->getParam("wifi_pass", true)->value().equals("********")) {
      WIFI_PASS = request->getParam("wifi_pass", true)->value();
    }
    String tmpip = request->getParam("web_ip", true)->value();
    String tmpwport = request->getParam("web_port", true)->value();
    String tmpsubn = request->getParam("subnet", true)->value();
    String WIFI_HOSTNAME = request->getParam("wifi_host", true)->value();
    String tmpua = "false";
    String tmpcw = "false";
    String tmpslp = "false";
    String tmpled = "false";
    String tmpfan = "false";
    if (request->hasParam("useap", true)) { tmpua = "true"; }
    if (request->hasParam("usewifi", true)) { tmpcw = "true"; }
    if (request->hasParam("espsleep", true)) { tmpslp = "true"; }
    if (request->hasParam("fanthres", true)) { tmpfan = "true"; }
    if (request->hasParam("ledstatus", true)) { tmpled = "true"; }
    if (tmpua.equals("false") && tmpcw.equals("false")) { tmpua = "true"; }
    int USB_WAIT = request->getParam("usbwait", true)->value().toInt();
    int TIME2SLEEP = request->getParam("sleeptime", true)->value().toInt();
    int TEMPERATURE = request->getParam("tempc", true)->value().toInt();
    File iniFile = FILESYS.open("/config.ini", "w");
    if (iniFile) {
      iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + tmpip + "\r\nWEBSERVER_PORT=" + tmpwport + "\r\nSUBNET_MASK=" + tmpsubn + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + USB_WAIT + "\r\nESPSLEEP=" + tmpslp + "\r\nSLEEPTIME=" + TIME2SLEEP + "\r\nLEDSTATUS="+ tmpled + "\r\nFANTHRES=" + tmpfan + "\r\nTEMPC=" + TEMPERATURE + "\r\n");
      iniFile.close();
    }
    String htmStr = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">#loader {z-index: 1;width: 50px;height: 50px;margin: 0 0 0 0;border: 6px solid #f3f3f3;border-radius: 50%;border-top: 6px solid #3498db;width: 50px;height: 50px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite; } @-webkit-keyframes spin {0%{-webkit-transform: rotate(0deg);}100%{-webkit-transform: rotate(360deg);}}@keyframes spin{0%{ transform: rotate(0deg);}100%{transform: rotate(360deg);}}body {background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;} #msgfmt {font-size: 16px; font-weight: normal;}#status {font-size: 16px; font-weight: normal;}</style></head><center><br><br><br><br><br><p id=\"status\"><div id='loader'></div><br>Config saved<br>Rebooting</p></center></html>";
    request->send(200, "text/html", htmStr);
    delay(1000);
    ESP.restart();
  } else {
    request->redirect("/config.html");
  }
}

void handleReboot(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", rebooting_gz, sizeof(rebooting_gz));
  response->addHeader("Content-Encoding", "gzip");
  request->send(response);
  delay(1000);
  ESP.restart();
}


void handleConfigHtml(AsyncWebServerRequest *request) {
  String tmpUa = "";
  String tmpCw = "";
  String tmpSlp = "";
  String tmpLed = "";
  String tmpFan = "";
  if (startAP) { tmpUa = "checked"; }
  if (connectWifi) { tmpCw = "checked"; }
  if (espSleep) { tmpSlp = "checked"; }
  if (ledStatus) { tmpLed = "checked"; }
  if (fanThres) { tmpFan = "checked"; }

  String htmStr = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Config Editor</title><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 14px;font-weight: bold;margin: 0 0 0 0.0;padding: 0.4em 0.4em 0.4em 0.6em;}input[type=\"submit\"]:hover {background: #ffffff;color: green;}input[type=\"submit\"]:active{outline-color: green;color: green;background: #ffffff; }table {font-family: arial, sans-serif;border-collapse: collapse;}td {border: 1px solid #dddddd;text-align: left;padding: 8px;}th {border: 1px solid #dddddd; background-color:gray;text-align: center;padding: 8px;}</style></head><body><form action=\"/config.html\" method=\"post\"><center><table><tr><th colspan=\"2\"><center>Access Point</center></th></tr><tr><td>AP SSID:</td><td><input required name=\"ap_ssid\" value=\"" + AP_SSID + "\"></td></tr><tr><td>AP PASSWORD:</td><td><input name=\"ap_pass\" value=\"********\"></td></tr><tr><td>AP IP:</td><td><input required name=\"web_ip\" value=\"" + Server_IP.toString() + "\"></td></tr><tr><td>SUBNET MASK:</td><td><input required name=\"subnet\" value=\"" + Subnet_Mask.toString() + "\"></td></tr><tr><td>START AP:</td><td><input type=\"checkbox\" name=\"useap\" " + tmpUa + "></td></tr><tr><th colspan=\"2\"><center>Web Server</center></th></tr><tr><td>WEBSERVER PORT:</td><td><input required type=\"number\" min=\"0\" max=\"65535\" name=\"web_port\" value=\"" + String(WEB_PORT) + "\"></td></tr><tr><th colspan=\"2\"><center>Wifi Connection</center></th></tr><tr><td>WIFI SSID:</td><td><input name=\"wifi_ssid\" value=\"" + WIFI_SSID + "\"></td></tr><tr><td>WIFI PASSWORD:</td><td><input name=\"wifi_pass\" value=\"********\"></td></tr><tr><td>WIFI HOSTNAME:</td><td><input name=\"wifi_host\" value=\"" + WIFI_HOSTNAME + "\"></td></tr><tr><td>CONNECT WIFI:</td><td><input type=\"checkbox\" name=\"usewifi\" " + tmpCw + "></td></tr><tr><th colspan=\"2\"><center>Auto USB Wait</center></th></tr><tr><td>WAIT TIME(ms):</td><td><input required type=\"number\" min=\"2000\" max =\"10000\" name=\"usbwait\" value=\"" + USB_WAIT + "\"></td></tr><tr><th colspan=\"2\"><center>ESP Sleep Mode</center></th></tr><tr><td>ENABLE SLEEP:</td><td><input type=\"checkbox\" name=\"espsleep\" " + tmpSlp + "></td></tr><tr><td>TIME TO SLEEP(minutes):</td><td><input required type=\"number\" min=\"5\" name=\"sleeptime\" value=\"" + TIME2SLEEP + "\"></td></tr><tr><th colspan=\"2\"><center>ESP LED Indicator</center></th></tr><tr><td>LED ON:</td><td><input type=\"checkbox\" name=\"ledstatus\" " + tmpLed + "></td></tr><tr><th colspan=\"2\"><center>FAN THRESHOLD</center></th></tr><tr><td>ENABLE FAN THRES:</td><td><input type=\"checkbox\" name=\"fanthres\" " + tmpFan + "></td></tr><tr><td>TEMP(celcius):</td><td><input required type=\"number\" min=\"50\" max=\"80\" name=\"tempc\" value=\"" + TEMPERATURE + "\"></td></tr></table><br><input id=\"savecfg\" type=\"submit\" value=\"Save Config\"></center></form></body></html>";
  request->send(200, "text/html", htmStr);
}

void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    String path = request->url();
    if (path != "/upload.html") {
      request->send(500, "text/plain", "Internal Server Error");
      return;
    }
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    if (filename.equals("/config.ini")) { return; }
    upFile = FILESYS.open(filename, "w");
  }
  if (upFile) {
    upFile.write(data, len);
  }
  if (final) {
    upFile.close();
  }
}


void handleConsoleUpdate(String rgn, AsyncWebServerRequest *request) {
  String Version = "05.050.000";
  String sVersion = "05.050.000";
  String lblVersion = "5.05";
  String imgSize = "0";
  String imgPath = "";
  String xmlStr = "<?xml version=\"1.0\" ?><update_data_list><region id=\"" + rgn + "\"><force_update><system level0_system_ex_version=\"0\" level0_system_version=\"" + Version + "\" level1_system_ex_version=\"0\" level1_system_version=\"" + Version + "\"/></force_update><system_pup ex_version=\"0\" label=\"" + lblVersion + "\" sdk_version=\"" + sVersion + "\" version=\"" + Version + "\"><update_data update_type=\"full\"><image size=\"" + imgSize + "\">" + imgPath + "</image></update_data></system_pup><recovery_pup type=\"default\"><system_pup ex_version=\"0\" label=\"" + lblVersion + "\" sdk_version=\"" + sVersion + "\" version=\"" + Version + "\"/><image size=\"" + imgSize + "\">" + imgPath + "</image></recovery_pup></region></update_data_list>";
  request->send(200, "text/xml", xmlStr);
}

void handleInfo(AsyncWebServerRequest *request) {
  float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
  FlashMode_t ideMode = ESP.getFlashChipMode();
  String mcuType = CONFIG_IDF_TARGET;
  mcuType.toUpperCase();
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>System Information</title><link rel=\"stylesheet\" href=\"style.css\"></head>";
  output += "<hr>###### Software ######<br><br>";
  output += "Firmware version " + firmwareVer + "<br>";
  output += "Creator: Stooged modified by zeusps3<br>";
  output += "SDK version: " + String(ESP.getSdkVersion()) + "<br><hr>";
  output += "###### Board ######<br><br>";
  output += "MCU: " + mcuType + "<br>";
#if defined(USB_PRODUCT)
  output += "Board: " + String(USB_PRODUCT) + "<br>";
#endif
  output += "Chip Id: " + String(ESP.getChipModel()) + "<br>";
  output += "CPU frequency: " + String(ESP.getCpuFreqMHz()) + "MHz<br>";
  output += "Cores: " + String(ESP.getChipCores()) + "<br><hr>";
  output += "###### Flash chip information ######<br><br>";
  output += "Flash chip Id: " + String(ESP.getFlashChipMode()) + "<br>";
  output += "Estimated Flash size: " + formatBytes(ESP.getFlashChipSize()) + "<br>";
  output += "Flash frequency: " + String(flashFreq) + " MHz<br>";
  output += "Flash write mode: " + String((ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT"
                                                                     : ideMode == FM_DIO  ? "DIO"
                                                                     : ideMode == FM_DOUT ? "DOUT"
                                                                                          : "UNKNOWN"))
            + "<br><hr>";
  output += "###### Storage information ######<br><br>";
  output += "Filesystem: SPIFFS<br>";
  output += "Total Size: " + formatBytes(FILESYS.totalBytes()) + "<br>";
  output += "Used Space: " + formatBytes(FILESYS.usedBytes()) + "<br>";
  output += "Free Space: " + formatBytes(FILESYS.totalBytes() - FILESYS.usedBytes()) + "<br><hr>";
  if (ESP.getPsramSize() > 0) {
    output += "###### PSRam information ######<br><br>";
    output += "Psram Size: " + formatBytes(ESP.getPsramSize()) + "<br>";
    output += "Free psram: " + formatBytes(ESP.getFreePsram()) + "<br>";
    output += "Max alloc psram: " + formatBytes(ESP.getMaxAllocPsram()) + "<br><hr>";
  }
  output += "###### Ram information ######<br><br>";
  output += "Ram size: " + formatBytes(ESP.getHeapSize()) + "<br>";
  output += "Free ram: " + formatBytes(ESP.getFreeHeap()) + "<br>";
  output += "Max alloc ram: " + formatBytes(ESP.getMaxAllocHeap()) + "<br><hr>";
  output += "###### Sketch information ######<br><br>";
  output += "Sketch hash: " + ESP.getSketchMD5() + "<br>";
  output += "Sketch size: " + formatBytes(ESP.getSketchSize()) + "<br>";
  output += "Free space available: " + formatBytes(ESP.getFreeSketchSpace() - ESP.getSketchSize()) + "<br><hr>";
  output += "###### Exploits information ######<br><br>";
  output += "GoldHen version: 2.4.b16.2<br>";
  output += "Webkit: PSFRee 1.4.0 beta<br>";
  output += "Kernel: p00bS4 Stooged<br><hr>";
  output += "</html>";
  request->send(200, "text/html", output);
}


void writeConfig() {
  File iniFile = FILESYS.open("/config.ini", "w");
  if (iniFile) {
    String tmpua = "false";
    String tmpcw = "false";
    String tmpslp = "false";
    String tmpled = "false";
    String tmpfan = "false";
    if (startAP) { tmpua = "true"; }
    if (connectWifi) { tmpcw = "true"; }
    if (espSleep) { tmpslp = "true"; }
    if (ledStatus) { tmpled = "true"; }
    if (fanThres) { tmpfan = "true"; }
    iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + Server_IP.toString() + "\r\nWEBSERVER_PORT=" + String(WEB_PORT) + "\r\nSUBNET_MASK=" + Subnet_Mask.toString() + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + USB_WAIT + "\r\nESPSLEEP=" + tmpslp + "\r\nSLEEPTIME=" + TIME2SLEEP + "\r\nLEDSTATUS="+ tmpled + "\r\nFANTHRES=" + tmpfan + "\r\nTEMPC=" + TEMPERATURE + "\r\n");
    iniFile.close();
  }
}

void setup() {

if (ledStatus) {
  pinMode(PowerPin, OUTPUT);
  millisBak = 0;
  ledOn=true;
}
if (FILESYS.begin(true)) {
    if (FILESYS.exists("/config.ini")) {
      File iniFile = FILESYS.open("/config.ini", "r");
      if (iniFile) {
        String iniData;
        while (iniFile.available()) {
          char chnk = iniFile.read();
          iniData += chnk;
        }
        iniFile.close();

        if (instr(iniData, "AP_SSID=")) {
          AP_SSID = split(iniData, "AP_SSID=", "\r\n");
          AP_SSID.trim();
        }

        if (instr(iniData, "AP_PASS=")) {
          AP_PASS = split(iniData, "AP_PASS=", "\r\n");
          AP_PASS.trim();
        }

        if (instr(iniData, "WEBSERVER_IP=")) {
          String strwIp = split(iniData, "WEBSERVER_IP=", "\r\n");
          strwIp.trim();
          Server_IP.fromString(strwIp);
        }

        if (instr(iniData, "SUBNET_MASK=")) {
          String strsIp = split(iniData, "SUBNET_MASK=", "\r\n");
          strsIp.trim();
          Subnet_Mask.fromString(strsIp);
        }

        if (instr(iniData, "WIFI_SSID=")) {
          WIFI_SSID = split(iniData, "WIFI_SSID=", "\r\n");
          WIFI_SSID.trim();
        }

        if (instr(iniData, "WIFI_PASS=")) {
          WIFI_PASS = split(iniData, "WIFI_PASS=", "\r\n");
          WIFI_PASS.trim();
        }

        if (instr(iniData, "WIFI_HOST=")) {
          WIFI_HOSTNAME = split(iniData, "WIFI_HOST=", "\r\n");
          WIFI_HOSTNAME.trim();
        }

        if (instr(iniData, "USEAP=")) {
          String strua = split(iniData, "USEAP=", "\r\n");
          strua.trim();
          if (strua.equals("true")) {
            startAP = true;
          } else {
            startAP = false;
          }
        }

        if (instr(iniData, "CONWIFI=")) {
          String strcw = split(iniData, "CONWIFI=", "\r\n");
          strcw.trim();
          if (strcw.equals("true")) {
            connectWifi = true;
          } else {
            connectWifi = false;
          }
        }

        if (instr(iniData, "USBWAIT=")) {
          String strusw = split(iniData, "USBWAIT=", "\r\n");
          strusw.trim();
          USB_WAIT = strusw.toInt();
        }

        if (instr(iniData, "ESPSLEEP=")) {
          String strsl = split(iniData, "ESPSLEEP=", "\r\n");
          strsl.trim();
          if (strsl.equals("true")) {
            espSleep = true;
          } else {
            espSleep = false;
          }
        }

        if (instr(iniData, "SLEEPTIME=")) {
          String strslt = split(iniData, "SLEEPTIME=", "\r\n");
          strslt.trim();
          TIME2SLEEP = strslt.toInt();
        }

        if (instr(iniData, "LEDSTATUS=")) {
          String strsl = split(iniData, "LEDSTATUS=", "\r\n");
          strsl.trim();
          if (strsl.equals("true")) {
            ledStatus = true;
          } else {
            ledStatus = false;
          }
        }

        if (instr(iniData, "FANTHRES=")) {
          String strsl = split(iniData, "FANTHRES=", "\r\n");
          strsl.trim();
          if (strsl.equals("true")) {
            fanThres = true;
          } else {
            fanThres = false;
          }
        }

        if (instr(iniData, "TEMPC=")) {
          String strslt = split(iniData, "TEMPC=", "\r\n");
          strslt.trim();
          TEMPERATURE = strslt.toInt();
        }

      }
    } else {
      writeConfig();
    }
  } else {

  }

  if (startAP) {
    WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());
    dnsServer.setTTL(30);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "*", Server_IP);
  }

  if (connectWifi && WIFI_SSID.length() > 0 && WIFI_PASS.length() > 0) {
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.hostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    } else {
      IPAddress LAN_IP = WiFi.localIP();
      if (LAN_IP) {
        String mdnsHost = WIFI_HOSTNAME;
        mdnsHost.replace(".local", "");
        MDNS.begin(mdnsHost.c_str());
        if (!startAP) {
          dnsServer.setTTL(30);
          dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
          dnsServer.start(53, "*", LAN_IP);
        }
      }
    }
  }

  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Microsoft Connect Test");
  });

  server.on("/config.ini", HTTP_ANY, [](AsyncWebServerRequest *request) {
    request->send(404);
  });

  server.on("/upload.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", upload_gz, sizeof(upload_gz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on(
    "/upload.html", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->redirect("/fileman.html");
    },
    handleFileUpload);

  server.on("/fileman.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleFileMan(request);
  });

  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    handleDelete(request);
  });

  server.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleConfigHtml(request);
  });

  server.on("/config.html", HTTP_POST, [](AsyncWebServerRequest *request) {
    handleConfig(request);
  });

  server.on("/reboot.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", reboot_gz, sizeof(reboot_gz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/reboot.html", HTTP_POST, [](AsyncWebServerRequest *request) {
    handleReboot(request);
  });

  server.on("/info.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleInfo(request);
  });

  server.on("/usbon", HTTP_POST, [](AsyncWebServerRequest *request) {
    enableUSB();
  });

  server.on("/usboff", HTTP_POST, [](AsyncWebServerRequest *request) {
    disableUSB();
  });

  server.on("/usbwait", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(USB_WAIT));
  });

  server.on("/tempc", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(TEMPERATURE));
  });

  server.on("/fan", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (fanThres) {
      request->send(200, "text/plain", "on");
    } else {
      request->send(200, "text/plain", "off");
    }    
  });

  server.on("/sleep", HTTP_POST, [](AsyncWebServerRequest *request) {
    handleSleep();
  });

  server.on("/format.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", format_gz, sizeof(format_gz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.on("/format.html", HTTP_POST, [](AsyncWebServerRequest *request) {
    isFormating = true;
    request->send(304);
  });

  server.on("/dlall", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleDlFiles(request);
  });

  server.on("/jzip.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", jzip_gz, sizeof(jzip_gz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  server.serveStatic("/", FILESYS, "/").setDefaultFile("index.html");

  server.onNotFound([](AsyncWebServerRequest *request) {
    String path = request->url();
    if (instr(path, "/update/ps4/")) {
      String Region = split(path, "/update/ps4/list/", "/");
      handleConsoleUpdate(Region, request);
      return;
    }
    if (instr(path, "/document/") && instr(path, "/ps4/")) {
      request->redirect("http://" + WIFI_HOSTNAME + "/loader.html");
      return;
    }
    if (path.endsWith("index.html") || path.endsWith("index.htm") || path.endsWith("/")) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_gz, sizeof(index_gz));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    if (path.endsWith("loader.html") || path.endsWith("loader.htm")) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", loader_gz, sizeof(loader_gz));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    if (path.endsWith("goldhen.bin")) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "application/octet-stream", goldhen_gz, sizeof(goldhen_gz));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    if (path.endsWith("style.css")) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", style_gz, sizeof(style_gz));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    if (path.endsWith("api.js")) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/js", api_gz, sizeof(api_gz));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    if (path.endsWith("kernel.js")) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/js", kernel_gz, sizeof(kernel_gz));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    if (path.endsWith("webkit.js")) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/js", webkit_gz, sizeof(webkit_gz));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    request->send(404);
  });
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();
  bootTime = millis();
}


static int32_t onRead(uint32_t lba, uint32_t offset, void * buffer, uint32_t bufsize) {
  if (lba > 130) { lba = 130;}
  memcpy(buffer, exfathax[lba] + offset, bufsize);
    return bufsize;
}


void enableUSB() {  
  dev.vendorID("PS4");
  dev.productID("ESP32 Server");
  dev.productRevision("1.0");
  dev.onRead(onRead);
  dev.mediaPresent(true);
  dev.begin(8192, 512);
  USB.begin();
  hasEnabled = true;
}

void disableUSB() {
  dev.end();
}

void handleSleep()
{
  powerdown =true;
} 

void loop() {
  if (millis() - millisBak > 1000 && hasEnabled && ledStatus)
  {
    if (ledOn){
      digitalWrite(PowerPin, HIGH);
    } 
    else {
      digitalWrite(PowerPin, LOW);
    }        
    ledOn = !ledOn;
    millisBak = millis();
  }

  if (!isFormating && powerdown) {   
      disableUSB();        
      hasEnabled = false;
  }
  
  if (espSleep && !isFormating) {
    if (millis() >= (bootTime + (TIME2SLEEP * 60000))) {
      disableUSB();        
      hasEnabled = false;
    }
  }

  if (!hasEnabled){
      if (ledStatus){
        digitalWrite(PowerPin, LOW);
      }      
      esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
      esp_deep_sleep_start();
      return;
  }

  if (isFormating) {
    isFormating = false;
    FILESYS.end();
    FILESYS.format();
    FILESYS.begin(true);
    delay(1000);
    writeConfig();
  }
  dnsServer.processNextRequest();
}