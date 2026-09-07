#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"




cd "$BASE_DIR/services/RTC" || exit 1

echo "RTC LOCAL MODE. Commands: READ | SET YYYY-MM-DD HH:MM:SS | QUIT"



exec sudo python3 rtc_serve.py
