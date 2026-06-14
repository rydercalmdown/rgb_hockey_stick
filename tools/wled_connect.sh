#!/bin/bash
# Join WLED-AP, push WiFi + LED config, then rejoin home network.
set -e

WLED_AP="WLED-AP"
WLED_AP_PASS="wled1234"
WLED_IP="4.3.2.1"
IFACE="en0"
CFG_FILE="home_assistant/wled_data/cfg.json"
BUILD_CFG="build/wled_data/cfg.json"

# Read home WiFi from .env
if [ ! -f .env ]; then echo ".env not found"; exit 1; fi
HOME_SSID=$(grep WIFI_SSID .env | cut -d= -f2)
HOME_PASS=$(grep WIFI_PASSWORD .env | cut -d= -f2)

echo "Joining $WLED_AP..."
networksetup -setairportnetwork $IFACE "$WLED_AP" "$WLED_AP_PASS"

echo "Waiting for WLED to be reachable..."
for i in $(seq 1 20); do
  if curl -s --max-time 1 "http://$WLED_IP/json/info" >/dev/null 2>&1; then
    echo "WLED reachable."
    break
  fi
  sleep 1
done

echo "Pushing config..."
curl -s -X POST "http://$WLED_IP/json/cfg" \
  -H "Content-Type: application/json" \
  -d @"$BUILD_CFG"
echo ""

echo "Rebooting WLED..."
curl -s -X POST "http://$WLED_IP/json" \
  -H "Content-Type: application/json" \
  -d '{"rb":true}'
echo ""

echo "Rejoining $HOME_SSID..."
networksetup -setairportnetwork $IFACE "$HOME_SSID" "$HOME_PASS"

echo "Waiting for hockeystick to appear on network..."
for i in $(seq 1 30); do
  if ping -c 1 -W 1 hockeystick.local >/dev/null 2>&1; then
    echo "Device online at hockeystick.local"
    curl -s http://hockeystick.local/json/info | python3 -m json.tool 2>/dev/null | grep -E '"name"|"ver"|"ip"'
    exit 0
  fi
  sleep 1
done

echo "Timed out — check router for 'hockeystick' device."
