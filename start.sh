#!/bin/bash
set -e

# Ensure dependencies are installed
pip install websockets rich spdlog boost > /dev/null 2>&1 || true

if [ ! -f "build/trading-core" ]; then
    echo "Error: C++ binary not found. Please run ./build.sh first."
    exit 1
fi

echo "Starting Polymarket Arbitrage Trading Core with Dashboard Bridge..."

# Start the bridge in the background
# The bridge handles starting the trading-core
python3 dashboard_bridge.py > bridge.log 2>&1 &
BRIDGE_PID=$!

# Give the bridge a second to start the websocket server
sleep 2

echo "Starting CLI Dashboard..."
# Launch the dashboard which connects to the bridge's websocket
python3 cli_dashboard.py || python cli_dashboard.py

# Cleanup: kill the bridge (and core) when the dashboard is closed
echo "Shutting down..."
kill $BRIDGE_PID 2>/dev/null || true
pkill -f trading-core 2>/dev/null || true
