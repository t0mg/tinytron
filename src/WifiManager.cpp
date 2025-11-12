#include "WifiManager.h"

// Simple WiFi manager for ESP32 using AsyncWebServer and Preferences
// Inspired by https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/

const char *WifiManager::PARAM_INPUT_1 = "ssid";
const char *WifiManager::PARAM_INPUT_2 = "pass";

WifiManager::WifiManager(AsyncWebServer *server, Prefs *prefs, Battery *battery)
    : server(server), prefs(prefs), _battery(battery), subnet(255, 255, 0, 0), previousMillis(0) {}

void WifiManager::begin()
{
  // Load values from preferences
  String ssid = prefs->getSsid();
  String pass = prefs->getPass();

  if (initWiFi())
  {
    setupServer();
  }
  else
  {
    setupAccessPoint();
  }
}

bool WifiManager::initWiFi()
{
  String ssid = prefs->getSsid();
  String pass = prefs->getPass();

  if (ssid == "")
  {
    Serial.println("Undefined SSID.");
    return false;
  }

  // Stop DNS server if running
  dnsServer.stop();

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  // WiFi.setSleep(false);
  // WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.println(WiFi.localIP());
  return true;
}

void WifiManager::setupServer()
{
  server->begin();
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
             { request->send(200, "text/html", index_html_start, index_html_end - index_html_start); });

  server->on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request)
             {
              // DO NOT remove the -1 below, it is to avoid sending an extra null byte at the end
              request->send(200, "application/javascript", app_js_start, app_js_end - app_js_start - 1); });

  server->on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
             { request->send(200, "text/html", String((const char *)wifimanager_html_start, (size_t)(wifimanager_html_end - wifimanager_html_start))); });

  server->on("/brightness", HTTP_GET, [this](AsyncWebServerRequest *request)
             { request->send(200, "text/plain", String(prefs->getBrightness())); });

  server->on(
      "/brightness", HTTP_POST, [this](AsyncWebServerRequest *request)
      {
    if (request->hasParam("brightness", true))
    {
      String brightnessStr = request->getParam("brightness", true)->value();
      int brightness = brightnessStr.toInt();
      if (brightness >= 1 && brightness <= 255)
      {
        prefs->setBrightness(brightness);
        request->send(200, "text/plain", "OK");
      }
      else
      {
        request->send(400, "text/plain", "Invalid brightness value");
      }
    }
    else
    {
      request->send(400, "text/plain", "Missing brightness parameter");
    } });

  server->on("/voltage", HTTP_GET, [this](AsyncWebServerRequest *request)
             {
    float voltage = _battery->getVoltage();
    String jsonResponse = "{\"voltage\": " + String(voltage) + "}";
    request->send(200, "application/json", jsonResponse); });

  setupWifiPostHandler();
}

void WifiManager::setupAccessPoint()
{
  Serial.println("Setting AP (Access Point)");
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, NULL);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Configure DNS server for captive portal
  dnsServer.start(53, "*", WiFi.softAPIP());

  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
             { request->send(200, "text/html", String((const char *)wifimanager_html_start, (size_t)(wifimanager_html_end - wifimanager_html_start))); });

  server->on("/index", HTTP_GET, [](AsyncWebServerRequest *request)
             { request->send(200, "text/html", String((const char *)wifimanager_html_start, (size_t)(wifimanager_html_end - wifimanager_html_start))); });

  // Add captive portal handler for all other requests
  server->addHandler(new CaptiveRequestHandler(wifimanager_html_start, wifimanager_html_end));

  setupWifiPostHandler();
  server->begin();
}

void WifiManager::setupWifiPostHandler()
{
  server->on("/wifi", HTTP_POST, [this](AsyncWebServerRequest *request)
             {
        String new_ssid = "";
        String new_pass = "";

        int params = request->params();
        for(int i=0;i<params;i++){
            const AsyncWebParameter* p = request->getParam(i);
            if(p->isPost()){
                if (p->name() == PARAM_INPUT_1) {
                    new_ssid = p->value();
                }
                if (p->name() == PARAM_INPUT_2) {
                    new_pass = p->value();
                }
            }
        }

        if(new_ssid == "") {
            request->send(400, "text/plain", "Error: SSID is required");
            return;
        }

        // Save new configuration
        prefs->setSsid(new_ssid);
        prefs->setPass(new_pass);

        String response = "Configuration saved. Restarting...";
        request->send(200, "text/plain", response);
        
        // Let the response be sent before restarting
        delay(1000);
        ESP.restart(); });
}

bool WifiManager::isConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isAPMode()
{
  wifi_mode_t mode = WiFi.getMode();
  return (mode == WIFI_AP || mode == WIFI_AP_STA);
} 

IPAddress WifiManager::getIpAddress()
{
  if (isAPMode())
  {
    return WiFi.softAPIP();
  }
  if (!isConnected())
  {
    return IPAddress(0, 0, 0, 0);
  }
  return WiFi.localIP();
}

void WifiManager::handleClient()
{
  // Only process DNS requests if in AP mode
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
  {
    dnsServer.processNextRequest();
  }
}
