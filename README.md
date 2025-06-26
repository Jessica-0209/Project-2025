# Wi-Fi Events Publishing via MQTT from hostapd

# Overview

This project captures real-time Wi-Fi events such as probe requests and association events from hostapd, transforms them into structured JSON format, and publishes them over an MQTT broker. A subscriber listens to these events, stores them in a hash table (MAC-based), and allows viewing the hash table via a CLI client.

# Features

- UNIX Domain Sockets for Inter Process Commuincation between hostapd and program code
- MQTT Publisher fetches wifi events from hostapd, transforms the log to JSON, and pushes this JSON to broker at topic `wifi/events`
- MQTT Subscriber listens and stores events in a hash table
- Efficient MAC-based tracking using hash table
- Multi-threaded subscriber (stores log in hash table + listens for CLI command)
- Real-time event inspection via CLI

# Setup Instructions

## 1. Prerequisites and Installation

```
sudo apt update
sudo apt install -y build-essential libmosquitto-dev mosquitto libcjson-dev hostapd libnl-3-dev libnl-genl-3-dev net-tools iw
```

## 2. Clone the Repository
```
git clone https://github.com/Jessica-0209/Project-2025.git
cd Project-2025
```
## 3. Build the Project
```
make clean
make
```
##4. Build the hostapd
```
cp ../hostap/hostapd/defconfig ../hostap/hostapd/.config
cd ../hostap/hostapd && make
```
# Steps for Execution

## 1. Configure Wi-Fi AP Interface and Start Hostapd
(on one terminal path -> Project-2025/Project_2025/hostap/hostapd)

Dynamically fetch the wireless interface name and configure the AP Interface
```
sudo ./ap_setup
```

## 3. Start MQTT broker
```
mosquitto -c /etc/mosquitto/mosquitto.conf
```
## 4. Start MQTT publisher 
(on another terminal)
```
sudo ./bin/wifi_mqtt publisher
```
- Connects to hostapd via UNIX socket
- Parses log -> JSON
- Publishes to topic wifi/events

By default the info level logs will be displayed. If you want to view debug or warning or error logs, 
```
sudo ./bin/wifi_mqtt publisher debug
```
```
sudo ./bin/wifi_mqtt publisher warn
```
```
sudo ./bin/wifi_mqtt publisher error
```

## 5. Start MQTT subscriber 
(on another terminal)
```
sudo ./bin/wifi_mqtt subscriber
```
- Subscribes to wifi/events
- Creates and maintains hash table of devices using MAC address
- Listens for CLI commands via pthread

By default the info level logs will be displayed. If you want to view debug or warning or error logs,
```
sudo ./bin/wifi_mqtt subscriber debug
```
```
sudo ./bin/wifi_mqtt subscriber warn
```
```
sudo ./bin/wifi_mqtt subscriber error
```

## 6. Display Hash Table via CLI
(on another terminal)
```
sudo ./bin/client_details
```
- Connects to subscriber and prints stored MAC-based event records

## 7. Terminate all Processes
```
Ctrl+C
```
in all the terminals
