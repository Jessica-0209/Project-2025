#!/bin/bash

# Detect Wi-Fi interface
INTERFACE=$(iw dev | awk '$1=="Interface"{print $2}' | head -n 1)

if [ -z "$INTERFACE" ]; then
    echo "No wireless interface found."
    exit 1
fi

echo "Restarting NetworkManager..."
sudo systemctl restart NetworkManager
sleep 10

# Check Wi-Fi connection status
STATUS=$(nmcli -t -f GENERAL.STATE device show $INTERFACE | awk -F: '{print $2}' | tr -d ' ')

if echo "$STATUS" | grep -q "100"; then
    echo "Wi-Fi successfully connected on $INTERFACE."
else
    echo "Wi-Fi connection failed on $INTERFACE."
    exit 1
fi

# Update DNS to Google DNS
echo "nameserver 8.8.8.8" | sudo tee /etc/resolv.conf > /dev/null
