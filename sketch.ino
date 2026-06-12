/**
 * @file AirQualityMonitor.ino
 * @brief Demonstrates a basic air quality monitor using GasSensor, Buzzer, and CharacterLcdDisplay.
 *
 * This example showcases the integration of multiple sensor and actuator types within the
 * Modest-IoT Nano-Framework. It monitors gas levels, performs calibration, displays
 * status and PPM values on an LCD, and triggers a buzzer upon gas detection.
 * @date 2026-06-12
 * @version 1.0
 */

#include <Arduino.h>
#include "ModestIoT.h" // Umbrella header for framework components

// --- 1. CONCRETE APPLICATION TELEMETRY SCHEMA ---

/**
 * @brief Telemetry package for Air Quality Monitor data.
 * Contains gas concentration, detection status, and sensor mode.
 */
class AirQualityTelemetryPackage : public TelemetryPackage {
private:
    /// @brief Gas concentration in PPM.
    float gasConcentrationInPpm;
    /// @brief Flag indicating if gas is detected.
    bool gasDetected;
    /// @brief The operating mode of the sensor, cast from `GasSensor::OperatingMode`.
    int operatingMode;

public:
    AirQualityTelemetryPackage(float ppm, bool detected, GasSensor::OperatingMode mode)
        : gasConcentrationInPpm(ppm), gasDetected(detected), operatingMode(static_cast<int>(mode)) {}

    virtual ~AirQualityTelemetryPackage() override = default;

    /**
     * @brief Serializes the telemetry data into a JSON object.
     * @param serializationDestination The `JsonDocument` where data will be stored.
     */
    void serialize(JsonDocument& serializationDestination) const override {
        serializationDestination["gasPpm"] = this->gasConcentrationInPpm;
        serializationDestination["gasDetected"] = this->gasDetected;
        serializationDestination["sensorMode"] = this->operatingMode;
    }
};

// --- 2. APPLICATION MEDIATOR IMPLEMENTATION ---

/**
 * @brief Orchestrates the GasSensor, Buzzer, and CharacterLcdDisplay for air quality monitoring.
 *
 * This device manages the gas sensor's calibration and operational modes, updates
 * an LCD with real-time data, and provides audible alerts via a buzzer.
 */
class AirQualityMonitorDevice : public Device {
private:
    /// @brief The gas sensor instance.
    GasSensor gasSensor;
    /// @brief The buzzer instance for alarms.
    Buzzer alarmBuzzer;
    /// @brief The LCD display instance for status.
    CharacterLcdDisplay statusDisplay;
    /// @brief The MQTT gateway client reference.
    MqttGatewayClient& gatewayClient;

    /**
     * @brief Updates the LCD display with current sensor status and backlight based on gas detection.
     *
     * Depending on the sensor's `OperatingMode` (Raw, Calibrating, or Accurate),
     * it formats the appropriate text and displays it on the LCD.
     * Also controls the backlight based on whether gas is detected.
     */
    void updateOperationStatusDisplay() {
        char textBufferInternal[2][21]; // Max 20 chars + null terminator for 2 lines
        const char* textBufferPtrs[2];

        switch (gasSensor.getOperatingMode()) {
            case GasSensor::OperatingMode::RawMode:
                snprintf(textBufferInternal[0], sizeof(textBufferInternal[0]), "Mode: RAW");
                snprintf(textBufferInternal[1], sizeof(textBufferInternal[1]), "Raw Val: %d", gasSensor.getRawAnalogValue());
                break;
            case GasSensor::OperatingMode::CalibratingMode:
                snprintf(textBufferInternal[0], sizeof(textBufferInternal[0]), "Mode: CALIBRATING");
                snprintf(textBufferInternal[1], sizeof(textBufferInternal[1]), "Keep in clean air!");
                break;
            case GasSensor::OperatingMode::AccurateMode:
                snprintf(textBufferInternal[0], sizeof(textBufferInternal[0]), "Mode: ACCURATE");
                if (gasSensor.getTargetGasType() != GasSensor::GasType::None) {
                    snprintf(textBufferInternal[1], sizeof(textBufferInternal[1]), "%s: %.1f PPM",
                             getGasTypeName(gasSensor.getTargetGasType()),
                             gasSensor.getGasConcentrationInPpm());
                } else {
                    snprintf(textBufferInternal[1], sizeof(textBufferInternal[1]), "Rs/R0: %.2f", gasSensor.getRsR0Ratio());
                }
                break;
        }

        textBufferPtrs[0] = textBufferInternal[0];
        textBufferPtrs[1] = textBufferInternal[1];

        statusDisplay.setLineBuffer(textBufferPtrs); // Use the overloaded method
        statusDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);

        // Control backlight based on gas detection
        if (gasSensor.isGasDetected()) {
            statusDisplay.handle(CharacterLcdDisplay::TURN_BACKLIGHT_ON_COMMAND);
        } else {
            statusDisplay.handle(CharacterLcdDisplay::TURN_BACKLIGHT_OFF_COMMAND);
        }
    }

    /**
     * @brief Gets the string representation of a gas type.
     * @param type The `GasSensor::GasType`.
     * @return The gas type name as a string.
     */
    const char* getGasTypeName(GasSensor::GasType type) {
        switch (type) {
            case GasSensor::GasType::LPG: return "LPG";
            case GasSensor::GasType::Methane: return "CH4";
            case GasSensor::GasType::CarbonMonoxide: return "CO";
            case GasSensor::GasType::Alcohol: return "Alcohol";
            case GasSensor::GasType::Propane: return "Propane";
            case GasSensor::GasType::Hydrogen: return "H2";
            case GasSensor::GasType::Smoke: return "Smoke";
            case GasSensor::GasType::Ammonia: return "NH3";
            case GasSensor::GasType::Benzene: return "Benzene";
            case GasSensor::GasType::Toluene: return "Toluene";
            case GasSensor::GasType::Acetone: return "Acetone";
            default: return "Unknown";
        }
    }

protected:
    /**
     * @brief Processes telemetry data by sending it via MQTT.
     * @param rawQueueItemPayload Pointer to the queued telemetry payload.
     */
    void processQueuedTelemetryData(const TelemetryPackage* rawQueueItemPayload) override {
        if (rawQueueItemPayload != nullptr) {
            gatewayClient.sendTelemetryRecord(*rawQueueItemPayload);
        }
    }

public:
    /**
     * @brief Constructs the AirQualityMonitorDevice.
     * @param gasAnalogPin Analog pin for the gas sensor.
     * @param gasDigitalPin Digital pin for the gas sensor's threshold output.
     * @param buzzerPin GPIO pin for the buzzer.
     * @param lcdI2CAddress I2C address of the LCD.
     * @param lcdColumns Number of columns on the LCD.
     * @param lcdRows Number of rows on the LCD.
     * @param client MQTT Gateway Client reference.
     * @param samplingIntervalInMilliseconds Polling interval for sensors.
     * @param timerGroupIndex Hardware timer index.
     */
    AirQualityMonitorDevice(int gasAnalogPin, int gasDigitalPin, int buzzerPin,
                            uint8_t lcdI2CAddress, uint8_t lcdColumns, uint8_t lcdRows,
                            MqttGatewayClient& client,
                            unsigned long samplingIntervalInMilliseconds, uint8_t timerGroupIndex = 0)
        : Device(samplingIntervalInMilliseconds, timerGroupIndex),
          gasSensor(gasAnalogPin, gasDigitalPin, this, GasSensor::OperatingMode::RawMode), // Start in RawMode
          alarmBuzzer(buzzerPin, this),
          statusDisplay(lcdI2CAddress, lcdColumns, lcdRows, true, this), // Backlight on initially
          gatewayClient(client)
    {
        // Set handlers for sensors (this device acts as the EventHandler)
        gasSensor.setHandler(this);

        // Initialize asynchronous engine
        initializeAsynchronousEngine(10); // Queue depth of 10

        // Append sensors to the scheduler
        appendSensorToScheduler(&gasSensor, Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
    }

    /**
     * @brief Handles incoming events from sensors.
     * @param event The triggered event context payload.
     */
    void on(Event event) override {
        if (event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER ||
            event.identifier == GasSensor::DIGITAL_GAS_DETECTED_EVENT_IDENTIFIER) {

            updateOperationStatusDisplay(); // Renamed from updateLcd

            // Control buzzer based on gas detection status
            if (gasSensor.isGasDetected()) {
                alarmBuzzer.setTone(2000, 100); // 2kHz for 100ms
                alarmBuzzer.handle(Buzzer::PLAY_TONE_COMMAND);
            } else {
                alarmBuzzer.handle(Buzzer::TURN_OFF_COMMAND);
            }

            // Enqueue telemetry
            TelemetryPackage* telemetry = new AirQualityTelemetryPackage(
                gasSensor.getGasConcentrationInPpm(),
                gasSensor.isGasDetected(),
                gasSensor.getOperatingMode()
            );
            if (!enqueueTelemetryPayload(&telemetry)) {
                delete telemetry; // Clean up if queue is full
            }
        }

        if (gasSensor.getOperatingMode() == GasSensor::OperatingMode::CalibratingMode) {
            // During calibration, we want to update the LCD with instructions and status even if no new data is propagated
            updateOperationStatusDisplay();
        }
    }

    /**
     * @brief Handles incoming commands (not directly used by this device, but required by interface).
     * @param command The command to handle.
     */
    void handle(Command command) override {
        // Not directly handling commands for this example, but could be extended
    }

    /**
     * @brief Starts the gas sensor calibration process.
     */
    void startGasSensorCalibration() {
        gasSensor.startCalibration();
    }

    /**
     * @brief Sets the target gas type for PPM calculation.
     * @param gasType The GasType to target.
     */
    void setGasSensorTargetGas(GasSensor::GasType gasType) {
        gasSensor.setTargetGas(gasType);
    }
};

// --- 3. TOP-LEVEL DECLARATIVE CANVAS (Arduino Setup & Loop) ---

/** @brief Hardware Pin Definitions */
/// @brief Analog pin for the gas sensor (ADC input).
static const int GAS_ANALOG_PIN = 34; // Example: ESP32 ADC1_CH6
/// @brief Digital pin for the gas sensor's threshold output (digital I/O).
static const int GAS_DIGITAL_PIN = 2; // Example: ESP32 GPIO2 (D0 on some MQ boards)
/// @brief GPIO pin for the buzzer (PWM output).
static const int BUZZER_PIN = 13; // Example: ESP32 GPIO13

/** @brief LCD Display Configurations */
/// @brief I2C address of the LCD module (e.g., 0x27 or 0x3F).
static const uint8_t LCD_I2C_ADDRESS = 0x27; 
/// @brief Number of columns on the LCD (e.g., 16 or 20).
static const uint8_t LCD_COLUMNS = 16;
/// @brief Number of rows on the LCD (e.g., 2 or 4).
static const uint8_t LCD_ROWS = 2;

/** @brief Network and MQTT Configuration */
/// @brief WiFi SSID for network connection.
static const char* WIFI_SSID = "Wokwi-GUEST";
/// @brief WiFi password for network connection.
static const char* WIFI_PASSWORD = "";
/// @brief MQTT broker IP address.
static const char* MQTT_BROKER_IP = "192.168.1.100"; 
/// @brief MQTT broker port.
static const uint16_t MQTT_BROKER_PORT = 1883;
/// @brief MQTT topic for publishing telemetry data.
static const char* MQTT_PUBLISH_TOPIC = "airquality/monitor/data";
/// @brief MQTT client ID to identify this device.
static const char* MQTT_CLIENT_ID = "AirQualityMonitor01";

// Framework Instance Pointers
static WiFiConnectivityDriver* wifiDriver = nullptr;
static MqttGatewayClient* mqttGateway = nullptr;
static AirQualityMonitorDevice* airMonitorDevice = nullptr;

/**
 * @brief Initializes the device, sensors, connectivity, and MQTT gateway.
 */
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000) { /* wait for serial port to connect */ }
    Serial.println("\n[AirQualityMonitor] Starting up...");

    // 1. Initialize Connectivity
    wifiDriver = new WiFiConnectivityDriver(WIFI_SSID, WIFI_PASSWORD);
    wifiDriver->connect(); // Start WiFi connection in background

    // 2. Initialize MQTT Gateway
    mqttGateway = new MqttGatewayClient(
        *wifiDriver,
        wifiDriver->getNetworkSocket(), // Use the exposed WiFiClient
        MQTT_BROKER_IP,
        MQTT_BROKER_PORT,
        MQTT_PUBLISH_TOPIC,
        MQTT_CLIENT_ID
    );

    // 3. Initialize Air Quality Monitor Device
    airMonitorDevice = new AirQualityMonitorDevice(
        GAS_ANALOG_PIN,
        GAS_DIGITAL_PIN,
        BUZZER_PIN,
        LCD_I2C_ADDRESS,
        LCD_COLUMNS,
        LCD_ROWS,
        *mqttGateway,
        1000 // Sample every 1 second
    );

    // 4. Configure Gas Sensor Calibration and Target Gas
    // IMPORTANT: Ensure the sensor is in clean air during calibration!
    airMonitorDevice->startGasSensorCalibration();
    airMonitorDevice->setGasSensorTargetGas(GasSensor::GasType::LPG); // Example: Target LPG for PPM calculation

    Serial.println("[AirQualityMonitor] Setup complete. Monitoring started.");
}

/**
 * @brief The main application loop, kept empty as the device runs asynchronously.
 */
void loop() {
    // The master loop is completely clean and empty.
    // It yields 100% of its execution cycles natively back to the FreeRTOS kernel scheduler.
    // A long delay here prevents watchdog timer resets in some FreeRTOS configurations
    // when the loop is otherwise completely idle.
    vTaskDelay(pdMS_TO_TICKS(60000));
}