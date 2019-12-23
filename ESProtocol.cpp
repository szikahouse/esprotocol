#include "ESProtocol.hpp"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>

ESProtocol::ESProtocol() {
  rpcIndex = 0;
  nextSysLogTime = 0;
}

void ESProtocol::initConfiguration() {
  strcpy(config.firmwareVersion, "0.0.0");

  // Every MQTT topic will be prefixed
  const char prefix[] = "device/";

  // Generate device name from Chip ID
  String deviceName = "ESP-" + String(ESP.getChipId(), 16);
  deviceName.toUpperCase();

  // Save device name to configuration
  strcpy(config.deviceName, deviceName.c_str());

  // Save RPC call topic to configuration
  strcpy(config.rpcCallTopic, prefix);
  strcat(config.rpcCallTopic, config.deviceName);
  strcat(config.rpcCallTopic, "/rpc/call");
  
  // Save RPC result topic to configuration
  strcpy(config.rpcResultTopic, prefix);
  strcat(config.rpcResultTopic, config.deviceName);
  strcat(config.rpcResultTopic, "/rpc/result");

  // Save event topic to configuration
  strcpy(config.eventTopic, prefix);
  strcat(config.eventTopic, config.deviceName);
  strcat(config.eventTopic, "/event");
  
  // Save log topic to configuration
  strcpy(config.logTopic, prefix);
  strcat(config.logTopic, config.deviceName);
  strcat(config.logTopic, "/log");
}

void ESProtocol::getFirmwareVersion(char* version) {
  strcpy(version, config.firmwareVersion);
}

void ESProtocol::setFirmwareVersion(const char* version) {
  strcpy(config.firmwareVersion, version);
}

void ESProtocol::registerSysRPCHandlers() {
  // Register RPC system log
  addRPCHandler("sysLog", [&](JsonObject& params, JsonObject& result) {
    this->rpcSysLog(result);
  });

  // Register RPC system reset
  addRPCHandler("sysReset", [](JsonObject& params, JsonObject& result) {
    ESP.reset();
  });

  // Register RPC system restart
  addRPCHandler("sysRestart", [](JsonObject& params, JsonObject& result) {
    ESP.restart();
  });
}

void ESProtocol::addRPCHandler(const char* name, std::function<void(JsonObject&, JsonObject&)> handler) {
  rpcHandlerNames[rpcIndex] = name;
  rcpHandlers[rpcIndex] = handler;
  rpcIndex++;
}

void ESProtocol::callRPCHandler(const char *name, JsonObject &params, JsonObject& result) {
  // Look up for method by name
  for (int i = 0; i < rpcIndex; i++) {
    if (! strcmp(name, rpcHandlerNames[i])) {
      rcpHandlers[i](params, result);
    }
  }
}

void ESProtocol::executeRPCMessage(char *message) {
  // Create call and result document
  DefaultJsonDocument callDocument;
  DefaultJsonDocument resultDocument;

  // Deserialize call document
  deserializeJson(callDocument, message);

  // Get JSON objects for params and result
  JsonObject params = callDocument["params"];
  JsonObject result = resultDocument.to<JsonObject>();

  // Call RPC method
  callRPCHandler(callDocument["method"].as<const char *>(), params, result);

  // Add the original ID of the call to the result to further identification
  result["id"] = callDocument["id"].as<int>();

  // Publish the result
  publishResult(result);
}

void ESProtocol::publish(const char* topic, JsonObject& data) {
  char message[ESPROTOCOL_STATIC_JSON_DOCUMENT_SIZE];
  serializeJson(data, message);
  mqttClient.publish(topic, message);
}

void ESProtocol::publishResult(JsonObject& result) {
  publish(config.rpcResultTopic, result);
}

void ESProtocol::emitEvent(JsonObject& event) {
  publish(config.eventTopic, event);
}

void ESProtocol::log(const char *message) {
  mqttClient.publish(config.logTopic, message);
}

void ESProtocol::configure() {
  initConfiguration();

  // Set WiFi hostname same with device name
  WiFi.hostname(config.deviceName);

  // Configure WiFi Manager
  WiFiManager wifiManager;
  wifiManager.setTimeout(180);
  wifiManager.autoConnect();

  // Retrieve MQTT configuration
  HTTPClient http;
  StaticJsonDocument<ESPROTOCOL_STATIC_JSON_DOCUMENT_SIZE> configDocument;
  String configUrl = "http://" + WiFi.gatewayIP().toString() + "/cgi-bin/esprotocol/get-config";
  http.begin(wifiClient, configUrl);
  http.GET();
  deserializeJson(configDocument, http.getString());

  // Configure MQTT
  mqttClient.setClient(wifiClient);
  mqttClient.setServer(configDocument["mqttHost"].as<const char *>(), configDocument["mqttPort"].as<int>());
  mqttClient.setCallback([&](char* topic, byte* payload, unsigned int length) {
    executeRPCMessage(reinterpret_cast<char *>(payload));
  });
}

void ESProtocol::connect() {
  mqttClient.connect(config.deviceName);
  mqttClient.subscribe(config.rpcCallTopic);
}

void ESProtocol::setup() {
  configure();
  connect();
  ArduinoOTA.begin();
  registerSysRPCHandlers();
}

void ESProtocol::loop() {
  mqttClient.loop();
  ArduinoOTA.handle();
}

void ESProtocol::rpcSysLog(JsonObject& result) {
  StaticJsonDocument<ESPROTOCOL_STATIC_JSON_DOCUMENT_SIZE> sysLogDocument;

  uint32_t heapFree;
  uint16_t heapMax;
  uint8_t heapFrag;

  ESP.getHeapStats(&heapFree, &heapMax, &heapFrag);

  // Build result
  result["fv"] = config.firmwareVersion;
  result["pv"] = ESPROTOCOL_VERSION;
  result["hf"] = heapFree;
  result["hm"] = heapMax;
  result["fs"] = ESP.getFlashChipSize();
  result["ss"] = ESP.getSketchSize();
}
