#include <Arduino.h>
#include <FastLED.h>

#include <WiFi.h>
#include <WebServer.h>

#include <WireGuard-ESP32.h>
const char *private_key = "iNUl+X4dfpNmCEwCg8qRlFk8GN9wxtl25V2JzKyWGE0=";
IPAddress local_ip(10, 31, 69, 212);
const char *public_key = "CYMMGLpaa29JrDThElrHqif0cViNPWs/BvY0szJRcD4=";
const char *endpoint_address = "185.194.142.8"; // IP of Wireguard endpoint to connect to.
int endpoint_port = 51820;
static WireGuard wg;

#include <Preferences.h>
Preferences preferences;

WebServer server(80);

#define NUM_LEDS 1
#define DATA_PIN 13
#define ONBOARD_LED 2
#define BUTTON_PIN 0

// Define the array of leds
CRGB leds[NUM_LEDS];
void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    Serial.println("WiFi lost connection");
    break;
  default:
    break;
  }
}
const char *ssid = "ESP32";
const char *pw = "";
const char *app_name = "example";

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  preferences.begin(app_name, false);

  // WiFi.config(serverIP, static_GW, static_SN);
  // register event handler
  WiFi.onEvent(WiFiEvent);

  // Initiate connection
  String ssid_str = preferences.getString("ssid");
  String pw_str = preferences.getString("pw");
  if (!ssid_str.isEmpty())
  {
    Serial.println("Found stored credentials");
    ssid = ssid_str.c_str();
    pw = pw_str.c_str();
  }
  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.print(" with password ");
  Serial.println(pw);
  if (pw_str.length() < 8)
    WiFi.begin(ssid);
  else
    WiFi.begin(ssid, pw);
  int connection_attempts = 0;
  while (!WiFi.isConnected())
  {
    connection_attempts++;
    if (connection_attempts > 100) // wait for 10 seconds
    {
      Serial.println("Connection failed, creating AP");
      WiFi.mode(WIFI_AP);
      if (pw_str.length() < 8)
        WiFi.softAP(ssid);
      else
        WiFi.softAP(ssid, pw);
      IPAddress IP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(IP);
      break;
    }
    delay(100);
  }
  Serial.print("Connected to WiFi with IP: ");
  Serial.println(WiFi.localIP());
  configTime(9 * 60 * 60, 0, "0.de.pool.ntp.org", "1.de.pool.ntp.org", "time.google.com");

  String endpoint_address_str = preferences.getString("endpoint_ad");
  if (!endpoint_address_str.isEmpty())
  {
    endpoint_address = endpoint_address_str.c_str();
  }
  if (preferences.getInt("endpoint_port", 0) > 0)
  {
    endpoint_port = preferences.getInt("endpoint_port");
  }
  String public_key_str = preferences.getString("public_key");
  if (!public_key_str.isEmpty())
  {
    public_key = public_key_str.c_str();
  }
  String private_key_str = preferences.getString("private_key");
  if (!private_key_str.isEmpty())
  {
    private_key = private_key_str.c_str();
  }
  String local_ip_str = preferences.getString("local_ip");
  if (!local_ip_str.isEmpty())
  {
    local_ip.fromString(local_ip_str.c_str());
  }

  wg.begin(
      local_ip,
      private_key,
      endpoint_address,
      public_key,
      endpoint_port);
  if (wg.is_initialized())
  {
    Serial.println("Connected to WireGuard");
  }

  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  server.on("/", []()
            { server.send(200, "text/plain", "hello from esp32!"); });

  server.on("/wifi", []()
            {
    if (server.hasArg("ssid"))
    {
      preferences.putString("ssid", server.arg("ssid"));
    }
    if (server.hasArg("pw"))
    {
      preferences.putString("pw", server.arg("pw"));
    }
    String response = "ssid: " + preferences.getString("ssid", "") + "\npw: " + preferences.getString("pw", "");
    server.send(200, "text/plain", "set wifi config to:\n" + response); });

  server.on("/wg", []()
            {
    if (server.hasArg("endpoint_address")) {
      preferences.putString("endpoint_ad", server.arg("endpoint_address"));
    }
    if (server.hasArg("endpoint_port")) {
      preferences.putInt("endpoint_port", server.arg("endpoint_port").toInt());
    }
    if (server.hasArg("private_key")) {
      preferences.putString("private_key", server.arg("private_key"));
    }
    if (server.hasArg("public_key")) {
      preferences.putString("public_key", server.arg("public_key"));
    }
    if (server.hasArg("local_ip")) {
      preferences.putString("local_ip", server.arg("local_ip"));
    }
    String response = "endpoint_address: " + preferences.getString("endpoint_ad", "") + "\nlocal_ip: " + preferences.getString("local_ip", "") + "\nendpoint_port: " + preferences.getInt("endpoint_port", 0) + "\nprivate_key: " + preferences.getString("private_key", "") + "\npublic_key: " + preferences.getString("public_key", "");
    server.send(200, "text/plain", "set wg config to:\n" + response); });

  server.on("/restart", []()
            {
    preferences.end();
    ESP.restart(); });

  server.on("/rgb", []()
            {
    if (server.hasArg("r"))
    {
      int r = server.arg("r").toInt();
      leds[0].r = r;
      preferences.putUChar("r", r);
    }
    if (server.hasArg("g"))
    {
      int g = server.arg("g").toInt();
      leds[0].g = g;
      preferences.putUChar("g", g);
    }
    if (server.hasArg("b"))
    {
      int b = server.arg("b").toInt();
      leds[0].b = b;
      preferences.putUChar("b", b);
    }
    server.send(200, "text/plain", String(leds[0].r) + " " + String(leds[0].g) + " " + String(leds[0].b)); });

  server.on("/brightness", []()
            {
    if (server.hasArg("brightness"))
    {
      String brightness = server.arg("brightness");
      FastLED.setBrightness(brightness.toInt());
      server.send(200, "text/plain", "brightness set to " + brightness);
      preferences.putUChar("brightness", brightness.toInt());
    }
    server.send(200, "text/plain", "please provide a brightness value"); });

  server.on("/led", []()
            {
    if (server.hasArg("led"))
    {
      String led = server.arg("led");
      if (led == "on")
      {
        digitalWrite(ONBOARD_LED, HIGH);
        server.send(200, "text/plain", "led on");
      }
      else if (led == "off")
      {
        digitalWrite(ONBOARD_LED, LOW);
        server.send(200, "text/plain", "led off");
      }
      else
      {
        server.send(200, "text/plain", "please provide a led value");
      }
    }
    else
    {
      server.send(200, "text/plain", "please provide a led value");
    } });

  server.on("/button", []()
            {
    if (digitalRead(BUTTON_PIN) == LOW)
    {
      server.send(200, "text/plain", "button pressed");
    }
    else
    {
      server.send(200, "text/plain", "button not pressed");
    } });

  server.onNotFound([]()
                    {
    Serial.println("Not found!");
    server.send(404, "text/plain", "404"); });

  server.begin();

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(preferences.getUChar("brightness", 5));
  leds[0].r = preferences.getUChar("r", 0);
  leds[0].g = preferences.getUChar("g", 0);
  leds[0].b = preferences.getUChar("b", 0);
}

void loop()
{
  server.handleClient();
  FastLED.delay(10);
}
