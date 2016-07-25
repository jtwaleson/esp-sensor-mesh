#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define BASE_SSID "your-home-wifi-ssid"
#define BASE_PASSWORD "your-home-wifi-password"
#define HOME_HTTP_SERVER "http://10.0.0.2:8000/"
#define MESH_INTERNAL_PASSWORD "thispasswordisnotsafe"

WiFiServer server(80);

int myDistance = -1;


void setup() {
  Serial.begin(115200);
  
  int numberOfNetworksFound = WiFi.scanNetworks(false,true);
  Serial.println("scan done");

  int maxnum = -1;
  if (numberOfNetworksFound == 0) {
    Serial.println("no networks found");
    return;
  } else {
    for (int i = 0; i < n; ++i)
    {
      String ssid = WiFi.SSID(i);
      if (ssid.startsWith("ESPMESH-")) {
        int num = ssid.substring(8).toInt();
        if (num > maxnum) {
          maxnum = num;
        }
      }
    }
  }

  // yes this is stupid, it forms a chain not a nice tree as described, but this way I can test
  // the other functionality while all modules are on my desk ...
  // in the proper algorithm they would all just connect to my home wifi station
  myDistance = maxnum + 1;

  String mySSID = "ESPMESH-" + String(myDistance);
  Serial.println("I will advertise myself as " + mySSID);

  WiFi.mode(WIFI_AP_STA);

  if (myDistance == 0) {
    // connect to "home" base network, we are at the edge
    WiFi.begin(BASE_SSID, BASE_PASSWORD);
  } else {
    // connect to the nearest hop
    String parentSSID = "ESPMESH-" + String(myDistance - 1);
    Serial.println("connecting as client to " + parentSSID);
    WiFi.begin(parentSSID.c_str(), MESH_INTERNAL_PASSWORD);
  }

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("waiting for connection");
    delay(100);
  }

  // start at 192.168.4.1, then 192.168.5.1, to avoid clashes, like standard ESP implementation
  IPAddress apIP(192, 168, 4 + myDistance, 1);
  IPAddress apGateway(192, 168, 4 + myDistance, 1);
  IPAddress apSubmask(255, 255, 255, 0);

  WiFi.softAPConfig(apIP, apGateway, apSubmask);
  WiFi.softAP(mySSID.c_str(), MESH_INTERNAL_PASSWORD);

  server.begin();
}

void handleIncomingHTTPRequest(WiFiClient client) {
  Serial.println("got incoming http request");

  String body = "";

  // stupid method to skip the http headers
  while (true) {
    String s = client.readStringUntil('\r');
    if (s.length() == 1) { // if only a \n is found it means the headers are done...
      client.read(); // drop the newline
      body = client.readString();
      break;
    } else if (s.length() == 0) {
      break;
    }
  }

  client.flush();
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/plain\r\n\r\n";
  s += "Great Success\r\n";
  client.print(s);
  client.close();

  if (body.length() > 0) {
    // TODO: put this in a transmit queue or something like that, now this will create a blocking process on all the hops until the edge of the network
    transmitSensorData(body);
  }
}

void transmitSensorData(String data) {
  HTTPClient http;
  if (myDistance == 0) {
    http.begin(HOME_HTTP_SERVER);
  } else {
    http.begin("http://192.168." + String(4 + myDistance - 1) + ".1/");
  }
  int code = http.POST(data);
  Serial.printf("transmitting sensor data, return code: %d\r\n", code);
}

int sleep = 0;
int messageId = 0;

void loop() {
  WiFiClient client = server.available();
  if (client) { 
    handleIncomingHTTPRequest(client);
  } else {
    delay(1);  // small delay so we don't wait one second before handling incoming http traffic
    sleep += 1;
    if (sleep >= 1000) {
      // generate some fake data
      transmitSensorData("client with distance: " + String(myDistance) + " - messageId: " + String(messageId));
      messageId += 1;
      sleep = 0;
    }
  }
}
