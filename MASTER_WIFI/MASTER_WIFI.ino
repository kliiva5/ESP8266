/*--------TIME LIBS ----------*/
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

/*-------- SENSOR LIBS ----------*/
#include <WiFiClientSecure.h>
#include <DHT.h>
#define DHTTYPE DHT11
#define DHTPIN 0
#define lightSensorPin A0

const char ssid[] = "TLU";  //  your network SSID (name)
const char pass[] = "";       // your network password
const char* host = "still-forest-62261.herokuapp.com";
const int httpsPort = 443;

String data = "";
int lightSensorValue = 0;
int temp_f = 0;
int light = 0;
int temp = 0;
int bulb = 0;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char* fingerprint = "CF 05 98 89 CA FF 8E D8 5E 5C E0 C2 E4 F7 E6 C3 C7 50 DD 5C";

DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure client;

// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
const int timeZone = 0;


WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

void setup()
{
  Serial.begin(115200);
  while (!Serial) ; // Needed for Leonardo only
  delay(250);
  Serial.println("TimeNTP Example");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
}

void loop() {
  
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  if (client.verify(fingerprint, host)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  temp_f = dht.readTemperature();
  lightSensorValue = analogRead(A0);

  String url = "https://still-forest-62261.herokuapp.com/save_data?light=";
  url += lightSensorValue;
  url += "&temp=";
  url += temp_f;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');

  light = (int) line[line.indexOf('ä') + 1];
  temp = (int) line[line.indexOf('ä') + 2];
  bulb = (int) line[line.indexOf('ä') + 3];

  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");

  if (light == '0') {
    digitalWrite(4, LOW);
  } else {
    digitalWrite(4, HIGH);
  } if (temp == '0') {
    digitalWrite(16, LOW);
  } else {
    digitalWrite(16, HIGH);
  } if (bulb == '0') {
    digitalWrite(5, LOW);
  } else {
    digitalWrite(5, HIGH);
  }

  Serial.println(get_timestring());
  line = "";
}


/*-------- Timestring ----------*/
String get_timestring() {
  String time_string;
  time_string += String(get_right_format(hour()));
  time_string += String(':');
  time_string += String(get_right_format(minute()));
  time_string += String(' ');
  time_string += String(get_right_format(day()));
  time_string += String('.');
  time_string += String(get_right_format(month()));
  time_string += String('.');
  time_string += String(year());
  return time_string;
}

String get_right_format(int digit){
  if(digit < 10){
    String new_digit = '0' + (String) digit;
    return new_digit;
  } else {
    return (String) digit;
  }
}

/*-------- NTP code ----------*/
/*-------- DO NOT TOUCH ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
