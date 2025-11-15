#include "WifiManager.h"
#include <ArduinoJson.h>
#include "AsyncJson.h"

// Simple WiFi manager for ESP32 using AsyncWebServer and Preferences
// Inspired by https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/

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

void WifiManager::setupCommonRoutes()
{
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
             { request->send(200, "text/html", index_html_start, index_html_end - index_html_start); });

  server->on("/vcr.ttf", HTTP_GET, [](AsyncWebServerRequest *request)
             { request->send(200, "text/html", vcr_ttf_start, vcr_ttf_end - vcr_ttf_start); });

  server->on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request)
             {
              // DO NOT remove the -1 below, it is to avoid sending an extra null byte at the end
              request->send(200, "application/javascript", app_js_start, app_js_end - app_js_start - 1); });

  server->on("/settings", HTTP_GET, [this](AsyncWebServerRequest *request)
             {
    JsonDocument json;
    json["ssid"] = prefs->getSsid();
    json["brightness"] = prefs->getBrightness();
    json["osdLevel"] = prefs->getOsdLevel();
    json["apMode"] = isAPMode();
    String response;
    serializeJson(json, response);
    request->send(200, "application/json", response); });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/settings", [this](AsyncWebServerRequest *request, JsonVariant &json)
                                                                          {
    JsonObject jsonObj = json.as<JsonObject>();
    bool restartRequired = false;

    if (jsonObj["ssid"].is<String>() && jsonObj["ssid"].as<String>() != prefs->getSsid()) {
        prefs->setSsid(jsonObj["ssid"].as<String>());
        restartRequired = true;
    }

    if (jsonObj["pass"].is<String>() && !jsonObj["pass"].as<String>().isEmpty()) {
        prefs->setPass(jsonObj["pass"].as<String>());
        restartRequired = true;
    }

    if (jsonObj["brightness"].is<int>()) prefs->setBrightness(jsonObj["brightness"].as<int>());
    if (jsonObj["osdLevel"].is<int>()) prefs->setOsdLevel(jsonObj["osdLevel"].as<int>());

    request->send(200, "application/json", "{\"status\":\"ok\"}");

    if (restartRequired) {
        delay(2000);
        ESP.restart();
    } });
  server->addHandler(handler);

  // OTA update endpoint
  server->on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    response->addHeader("Connection", "close");
    request->send(response);
    ESP.restart();
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
      Serial.printf("Update Start: %s\n", filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %uB\n", index + len);
      } else {
        Update.printError(Serial);
      }
    }
  });
}

void WifiManager::setupServer()
{
  server->begin();
  setupCommonRoutes();
  server->on("/voltage", HTTP_GET, [this](AsyncWebServerRequest *request)
             {
    float voltage = _battery->getVoltage();
    String jsonResponse = "{\"voltage\": " + String(voltage) + "}";
    request->send(200, "application/json", jsonResponse); });
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

  server->on("/index", HTTP_GET, [](AsyncWebServerRequest *request)
             { request->send(200, "text/html", index_html_start, index_html_end - index_html_start); });

  // Add captive portal handler for all other requests
  server->addHandler(new CaptiveRequestHandler(index_html_start, index_html_end));

  setupCommonRoutes();

  server->begin();
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
