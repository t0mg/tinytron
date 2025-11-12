#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include "Prefs.h"
#include "Battery.h"

// Embedded files
extern const uint8_t index_html_start[] asm("_binary_src_www_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_src_www_index_html_end");
extern const uint8_t app_js_start[] asm("_binary_src_www_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_src_www_app_js_end");
extern const uint8_t wifimanager_html_start[] asm("_binary_src_www_wifimanager_html_start");
extern const uint8_t wifimanager_html_end[] asm("_binary_src_www_wifimanager_html_end");

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler(const uint8_t *html_start, const uint8_t *html_end)
      : html_start(html_start), html_end(html_end) {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    String url = request->url();

    // A list of valid URLs that should not be redirected by the captive portal.
    const char *valid_urls[] = {"/", "wifi", "/favicon.ico", nullptr};
    for (const char *valid_url : valid_urls)
    {
      if (url == valid_url)
      {
        return false;
      }
    }

    // For any other URL, the captive portal will handle the request and redirect.
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request)
  {
    // Redirect to the WiFi configuration page
    AsyncWebServerResponse *response = request->beginResponse(302, "text/plain", "");
    response->addHeader("Location", "http://192.168.4.1");
    request->send(response);
  }

private:
  const uint8_t *html_start;
  const uint8_t *html_end;
};

class WifiManager
{
public:
  WifiManager(AsyncWebServer *server, Prefs *prefs, Battery *battery);
  void begin();
  bool isConnected();
  bool isAPMode();
  void handleClient();
  IPAddress getIpAddress();

private:
  static const char *PARAM_INPUT_1;
  static const char *PARAM_INPUT_2;

  Prefs *prefs;
  Battery *_battery;

  AsyncWebServer *server;
  IPAddress localIP;
  IPAddress localGateway;
  IPAddress subnet;
  DNSServer dnsServer;

  unsigned long previousMillis;
  static const long interval = 10000; // interval to wait for Wi-Fi connection (milliseconds)

  bool initWiFi();
  void setupServer();
  void setupAccessPoint();
  void setupWifiPostHandler();
};