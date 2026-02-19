#!/bin/sh
# Script pentru actualizarea URL-ului serverului

CONFIG_FILE="/etc/data-collector/server.conf"

if [ -z "$1" ]; then
    echo "Usage: $0 <server-url>"
    echo "Example: $0 http://10.8.0.1/data"
    echo ""
    echo "Current URL:"
    cat $CONFIG_FILE
    exit 1
fi

NEW_URL="$1"

echo "$NEW_URL" > $CONFIG_FILE
echo "✅ Server URL updated to: $NEW_URL"
echo ""
echo "Reloading data-collector configuration..."
PIDFILE="/var/run/data-collector.pid"

if [ -f "$PIDFILE" ]; then
    PID=$(cat "$PIDFILE")
    if kill -0 "$PID" 2>/dev/null; then
        kill -HUP "$PID"
        echo "Signal sent to process $PID"
    else
        echo "Process not running. Starting service..."
        /etc/init.d/data-collector start
    fi
else
    echo "PID file not found. Starting service..."
    /etc/init.d/data-collector start
fi

echo "Done!"