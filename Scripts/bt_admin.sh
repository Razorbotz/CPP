#!/bin/bash

# --- Configuration ---
LOG_FILE="./bt_admin.log"

# --- Logging Function ---
log_action() {
    local message="$1"
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $message" >> "$LOG_FILE"
}

# --- Help Function ---
show_help() {
    echo "================================================================"
    echo "Jetson Bluetooth Admin Utility (with Logging)"
    echo "================================================================"
    echo "Usage: ./bt_admin.sh [MAC_ADDRESS | FLAG]"
    echo ""
    echo "Flags:"
    echo "  --help         Show this help message"
    echo "  --status       Show local Bluetooth controller & paired devices"
    echo "  --reset        WIPE ALL paired devices and reset the controller"
    echo "  --discover     Make THIS Jetson discoverable for 60 seconds"
    echo ""
    echo "Standard Pairing:"
    echo "  ./bt_admin.sh AA:BB:CC:DD:EE:FF"
    echo ""
    echo "Logs are saved to: $LOG_FILE"
    echo "================================================================"
}

# --- Utility: Check Dependencies ---
check_deps() {
    if ! command -v expect &> /dev/null; then
        echo "[-] Error: 'expect' is not installed. Run: sudo apt install expect"
        log_action "FAILURE: Dependency 'expect' missing."
        exit 1
    fi
}

# --- Utility: Get Status ---
show_status() {
    echo "--- Local Controller ---"
    bluetoothctl show | grep -E "Name|Address|Powered|Discoverable"
    echo "--- Paired & Trusted Devices ---"
    bluetoothctl devices
    log_action "STATUS: User requested status check."
}

# --- Utility: Reset Bluetooth ---
reset_bluetooth() {
    echo "[!] WARNING: This will remove ALL paired Bluetooth devices."
    read -p "Are you sure? (y/N): " confirm
    if [[ "$confirm" == [yY] ]]; then
        devices=$(bluetoothctl devices | cut -d ' ' -f 2)
        for dev in $devices; do
            echo "Removing $dev..."
            bluetoothctl remove "$dev" > /dev/null
        done
        sudo systemctl restart bluetooth
        echo "[+] Bluetooth stack reset and all pairings cleared."
        log_action "RESET: All paired devices removed and service restarted."
    else
        echo "Reset cancelled."
    fi
}

# --- Utility: Discoverable Mode ---
set_discoverable() {
    echo "[+] Making device discoverable for 60 seconds..."
    log_action "DISCOVER: Device set to discoverable mode."
    sudo rfkill unblock bluetooth
    bluetoothctl power on
    bluetoothctl discoverable on
    bluetoothctl pairable on
    for i in {60..1}; do
        printf "\rRemaining: %2d seconds " $i
        sleep 1
    done
    bluetoothctl discoverable off
    echo -e "\n[!] Discoverable mode disabled."
}

# --- Execution Logic ---
check_deps

case "$1" in
    --help) show_help ;;
    --status) show_status ;;
    --reset) reset_bluetooth ;;
    --discover) set_discoverable ;;
    "") show_help ;;
    *)
        PEER_MAC=$1
        if [[ ! $PEER_MAC =~ ^([0-9A-Fa-f]{2}:){5}([0-9A-Fa-f]{2})$ ]]; then
            echo "[-] Error: Invalid MAC format."
            log_action "FAILURE: Invalid MAC format entered: $PEER_MAC"
            exit 1
        fi

        if bluetoothctl info "$PEER_MAC" | grep -q "Trusted: yes"; then
            echo "[+] $PEER_MAC is already trusted."
            log_action "SKIP: $PEER_MAC already trusted."
            exit 0
        fi

        echo "[*] Initiating pairing with $PEER_MAC..."
        log_action "START: Pairing attempt with $PEER_MAC"
        
        sudo rfkill unblock bluetooth
        bluetoothctl power on
        bluetoothctl agent on
        bluetoothctl default-agent

        # Automate the pairing with expect
        expect << EOF
            set timeout 30
            # Increase log level for debugging if needed: exp_internal 1
            spawn bluetoothctl
            expect "# "
            send "agent on\r"
            expect "Agent registered"
            send "default-agent\r"
            expect "Default agent request successful"
            
            send "scan on\r"
            expect "$PEER_MAC"
            send "scan off\r"
            expect "# "
            
            send "trust $PEER_MAC\r"
            expect "trust succeeded"
            
            send "pair $PEER_MAC\r"
            
            # The key is catching the specific Agent prompt
            expect {
                -re "Confirm passkey|Accept pairing|yes/no" { 
                    send "yes\r"
                    exp_continue 
                }
                -re "Enter PIN code" { 
                    send "0000\r"
                    exp_continue 
                }
                "Pairing successful" { 
                    # Success reached
                }
                timeout { 
                    puts "Timed out waiting for response"
                    exit 1 
                }
            }
            expect "# "
            send "quit\r"
            expect eof
EOF
        
        if [ $? -eq 0 ]; then
            echo "[+] Success!"
            log_action "SUCCESS: Paired and Trusted $PEER_MAC"
        else
            echo "[-] Pairing failed."
            log_action "FAILURE: Pairing timed out or rejected for $PEER_MAC"
        fi
        ;;
esac