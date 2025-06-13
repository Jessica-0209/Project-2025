# Wi-Fi Events Publishing via MQTT from hostapd

## Overview

This project captures real-time Wi-Fi events such as probe requests and association events from hostapd, transforms them into structured JSON format, and publishes them over an MQTT broker. A subscriber listens to these events, stores them in a hash table (MAC-based), and allows viewing the hash table via a CLI client.

## Features

- UNIX Domain Sockets for Inter Process Commuincation between hostapd and program code
- MQTT Publisher fetches wifi events from hostapd, converts the log to JSON, and pushes  to broker at topic `wifi/events`
- MQTT Subscriber listens and stores events in a hash table
- Efficient MAC-based tracking using hash table
- Multi-threaded subscriber (stores log in hash table + listens for CLI command)
- Real-time event inspection via CLI

## Setup Instructions

1. Prerequisites and Installation

```bash
sudo apt update
sudo apt install -y build-essential libmosquitto-dev mosquitto libcjson-dev hostapd libnl-3-dev libnl-genl-3-dev net-tools iw
```

2. Clone the Repository
```
git clone https://github.com/Jessica-0209/Project-2025.git
cd Project-2025
```
3. Build the Project
```
make clean
make
```
## Steps for Execution

1. Start hostapd (on one terminal)

sudo ./hostapd hostapd.conf

2. Configure Wi-Fi AP Interface

sudo systemctl stop NetworkManager
sudo ip link set <interface> down
sudo ip addr flush dev <interface>
sudo ip addr add 192.168.25.1/24 dev <interface>
sudo iw dev <interface> set type __ap
sudo ip link set <interface> up

Replace <interface> with your network interface name

3. Start MQTT broker

mosquitto -c /etc/mosquitto/mosquitto.conf

4. Start MQTT publisher (on another terminal)

sudo ./bin/wifi_mqtt publisher

Connects to hostapd via UNIX socket
Parses log → JSON
Publishes to topic wifi/events

5. Start MQTT subscriber (on another terminal)

sudo ./bin/wifi_mqtt subscriber

Subscribes to wifi/events
Creates and maintains hash table of devices using MAC address
Listens for CLI commands via pthread

4. Display Hash Table via CLI (on another terminal)

sudo ./bin/cli_client

Connects to subscriber and prints stored MAC-based event records

