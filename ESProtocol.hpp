#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

// Constants
#define ESPROTOCOL_VERSION "1.0.0"
#define ESPROTOCOL_MAX_RPC_METHODS 10
#define ESPROTOCOL_STATIC_JSON_DOCUMENT_SIZE 256

// Declare an alias for sized static JSON document
typedef StaticJsonDocument<ESPROTOCOL_STATIC_JSON_DOCUMENT_SIZE> DefaultJsonDocument;

// Configuration structure
typedef struct {
  char firmwareVersion[12];
  char deviceName[12];
  char rpcCallTopic[32];
  char rpcResultTopic[32];
  char eventTopic[32];
  char logTopic[32];
  char sysTopic[32];
} ESProtocolConfig;

class ESProtocol {
  // RPC methods
  std::function<void(JsonObject&, JsonObject&)> rcpHandlers[ESPROTOCOL_MAX_RPC_METHODS];

  // RPC method names
  const char* rpcHandlerNames[ESPROTOCOL_MAX_RPC_METHODS];

  // Current RPC count index
  int rpcIndex;

  // WiFi client
  WiFiClient wifiClient;

  // MQTT client
  PubSubClient mqttClient;

  // Configuration
  ESProtocolConfig config;

  // Next system log time
  unsigned long nextSysLogTime;

  /**
   * Initialize configuration.
   */
  void initConfiguration();

  /**
   * Configure the dependencies.
   */
  void configure();

  /**
   * Connect to related services.
   */
  void connect();

  /**
   * Publish a message.
   * 
   * @param topic MQTT topic to publish on.
   * @param data object to serialize and publish.
   */
  void publish(const char* topic, JsonObject& data);

  /**
   * Register system handlers.
   */
  void registerSysRPCHandlers();

  /**
   * RPC system log.
   * 
   * @param result JSON object containing system information.
   */
  void rpcSysLog(JsonObject& result);
public:
  /**
   * Private constructor to make sure we create a
   * single instace using getInstance() method.
   */
  ESProtocol();

  /**
   * Add and register RPC method.
   * 
   * @param name identifier / alias for the method.
   * @param handler method reference to register.
   */
  void addRPCHandler(const char* name, std::function<void(JsonObject&, JsonObject&)> handler);

  /**
   * Call an existing RPC method.
   * 
   * @param name identifier / alias for the method.
   * @param params JSON document containing the parame.
   * @param rsesult JSON document to pass forward to the RPC method for writing it's result.
   */
  void callRPCHandler(const char *name, JsonObject &params, JsonObject& result);

  /**
   * Execute a RPC call from a JSON message.
   * 
   * @param message JSON message containing the RPC call.
   */
  void executeRPCMessage(char *message);

  /**
   * Publish result.
   * 
   * @param result data to be published.
   */
  void publishResult(JsonObject& result);

  /**
   * Emit an event.
   * 
   * @param event data to be emitted.
   */
  void emitEvent(JsonObject& event);

  /**
   * Log a message.
   */
  void log(const char* message);

  /**
   * Get firmware version.
   * 
   * @param version output for version.
   */
  void getFirmwareVersion(char* version);

  /**
   * Set firmware version.
   * 
   * @param version firmware version to be set.
   */
  void setFirmwareVersion(const char* version);

  /**
   * Initialize the protocol.
   */
  void setup();

  /**
   * Looping method, called every time in the main loop.
   */
  void loop();
};
