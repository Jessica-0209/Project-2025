#!/bin/bash

# Get the first wireless interface
INTERFACE=$(iw dev | awk '$1=="Interface"{print $2}' | head -n 1)

if [ -z "$INTERFACE" ]; then
    echo "No wireless interface found."
    exit 1
fi

echo "Using wireless interface: $INTERFACE"

# Stop NetworkManager to free the interface
sudo systemctl stop NetworkManager

# Bring the interface down
sudo ip link set $INTERFACE down

# Clear existing IPs
sudo ip addr flush dev $INTERFACE

# Assign new IP
sudo ip addr add 192.168.25.1/24 dev $INTERFACE
#ADD A NOTE IN README FOR ASSIGNING IP ADDRESS
# Set interface to AP mode
sudo iw dev $INTERFACE set type __ap

# Bring the interface up
sudo ip link set $INTERFACE up

echo "Access Point setup complete on $INTERFACE"

# Start hostapd
echo "Starting hostapd..."
sudo ./hostapd hostapd.conf
