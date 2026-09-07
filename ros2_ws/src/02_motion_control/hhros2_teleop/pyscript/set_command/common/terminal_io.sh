#!/bin/bash

TTY_DEVICE="${TTY_DEVICE:-/dev/tty}"

read_key() {
    local var_name="$1"
    if [[ -r "$TTY_DEVICE" ]]; then
        IFS= read -r -s -n 1 "$var_name" < "$TTY_DEVICE"
    else
        IFS= read -r -s -n 1 "$var_name"
    fi
}

read_key_timeout() {
    local var_name="$1"
    local timeout="$2"
    if [[ -r "$TTY_DEVICE" ]]; then
        IFS= read -r -s -n 1 -t "$timeout" "$var_name" < "$TTY_DEVICE"
    else
        IFS= read -r -s -n 1 -t "$timeout" "$var_name"
    fi
}

read_key_prompt() {
    local var_name="$1"
    local prompt="$2"
    if [[ -r "$TTY_DEVICE" ]]; then
        IFS= read -r -n 1 -p "$prompt" "$var_name" < "$TTY_DEVICE"
    else
        IFS= read -r -n 1 -p "$prompt" "$var_name"
    fi
}

read_line() {
    local var_name="$1"
    local prompt="$2"
    if [[ -r "$TTY_DEVICE" ]]; then
        IFS= read -r -p "$prompt" "$var_name" < "$TTY_DEVICE"
    else
        IFS= read -r -p "$prompt" "$var_name"
    fi
}
