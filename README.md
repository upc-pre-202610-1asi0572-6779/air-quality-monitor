# Air Quality Monitor (ESP32 + Wokwi)

An ESP32-based air quality monitoring project that reads gas concentration data, displays status on an I2C LCD, triggers a buzzer alarm on detection, and publishes telemetry over MQTT.

## Overview

This project uses the ModestIoT framework to orchestrate:

- MQ-style gas sensor (analog + digital threshold output)
- 16x2 I2C LCD for live operating status
- Buzzer for audible alarm when gas is detected
- Wi-Fi + MQTT telemetry publishing

The source sketch is designed for asynchronous operation on ESP32 and runs in simulation with Wokwi.

## Features

- Multimode gas sensor workflow (`RawMode`, `CalibratingMode`, `AccurateMode`)
- On-device calibration flow and target gas selection
- Live LCD feedback for mode and gas readings
- Alarm buzzer on gas detection
- JSON telemetry payloads published via MQTT

## Hardware/Simulation Setup

From `diagram.json`, the main parts are:

- ESP32 DevKit v1
- Wokwi Gas Sensor
- Wokwi LCD1602 (I2C)
- Wokwi Buzzer

### Pin Mapping

- Gas Sensor analog output (`AOUT`) -> ESP32 `GPIO34`
- Gas Sensor digital output (`DOUT`) -> ESP32 `GPIO2`
- Buzzer -> ESP32 `GPIO13`
- LCD I2C `SDA` -> ESP32 `GPIO21`
- LCD I2C `SCL` -> ESP32 `GPIO22`

## Project Structure

- `sketch.ino`: Main firmware and application logic
- `diagram.json`: Wokwi wiring diagram
- `libraries.txt`: Wokwi library dependencies
- `wokwi-project.txt`: Wokwi project metadata

## Dependencies

Defined in `libraries.txt`:

- `ArduinoJson`
- `PubSubClient`
- `DHT sensor library`
- `ESP32Servo`
- `LiquidCrystal_I2C`
- `ModestIoT@wokwi:cf3b377520637d396b451edb39e21f8a3b80dd12`

## Configuration

Key settings are in `sketch.ino` as `static const` values:

- Wi-Fi credentials (`WIFI_SSID`, `WIFI_PASSWORD`)
- MQTT broker details (`MQTT_BROKER_IP`, `MQTT_BROKER_PORT`)
- MQTT topic/client ID (`MQTT_PUBLISH_TOPIC`, `MQTT_CLIENT_ID`)
- Sampling interval (`1000 ms` by default)

Update these constants before running against your own network/broker.

## Quick Start (Wokwi)

1. Open the project in Wokwi (see `wokwi-project.txt` for source URL).
2. Ensure dependencies from `libraries.txt` are available.
3. Run the simulation.
4. Open Serial Monitor at `115200` baud.

Expected behavior:

- Device boots and starts Wi-Fi/MQTT setup.
- Sensor calibration starts in clean air.
- LCD shows operating mode and readings.
- Buzzer sounds when gas detection is active.
- Telemetry records are sent as JSON over MQTT.

## Telemetry Format

Telemetry is serialized from `AirQualityTelemetryPackage` with these fields:

- `gasPpm` (float)
- `gasDetected` (bool)
- `sensorMode` (int enum value)

## Notes and Best Practices

- Perform calibration in clean air for better baseline accuracy.
- Keep MQTT credentials/secrets out of version control in production use.
- Validate sensor thresholds and gas-type calibration curves on real hardware.
- Use non-blocking/asynchronous patterns (already used in this sketch) for responsiveness.

## License

This project is licensed under the MIT License. See `LICENSE.md` for details.

