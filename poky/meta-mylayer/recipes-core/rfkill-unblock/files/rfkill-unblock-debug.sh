#!/bin/sh
### BEGIN INIT INFO
# Provides:          rfkill-unblock
# Required-Start:    $local_fs $network
# Required-Stop:     $local_fs
# Default-Start:     S
# Default-Stop:      0 6
# Short-Description: Unblock Wi-Fi interface (P99)
# Description:       Forces 'rfkill unblock wifi' and logs result to /var/log/rfkill_debug.log.
### END INIT INFO

LOG_FILE="/var/log/rfkill_debug.log"
RFKILL_BIN="/usr/sbin/rfkill"
CONNMAN_INIT_SCRIPT="/etc/init.d/connman"

log_message() {
    # Functie de logare pe disc, independenta de syslogd
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> $LOG_FILE
}

case "$1" in
    start)
        log_message "--- Starting rfkill Unblock Service (P99) ---"
        # ... (verificari initiale) ...
        
        # 3. Executa deblocarea Wi-Fi
        log_message "Executing: rfkill unblock wifi..."
        /usr/sbin/rfkill unblock wifi
        
        # Nou: Adaugă o scurtă pauză pentru stabilizarea driverului
        log_message "Adding 1-second delay for driver stabilization..."
        sleep 1
        
        /usr/sbin/rfkill unblock wifi
        RFKILL_STATUS=$?
        
        if [ $RFKILL_STATUS -eq 0 ]; then
            log_message "SUCCESS: Wi-Fi unblock command completed successfully."
        else
            log_message "FAILURE: rfkill unblock failed with status $RFKILL_STATUS."
        fi

        # 4. Reporneste Connman pentru a forta scanarea retelelor deblocate
        if [ -x $CONNMAN_INIT_SCRIPT ]; then
             log_message "Restarting Connman to apply changes..."
             $CONNMAN_INIT_SCRIPT restart
             log_message "Connman restart completed."
        fi

        # NOU: Verificare și deblocare finală, după restartul Connman.
        # Aceasta neutralizează orice re-blocare cauzată de serviciile Connman/networking.
        log_message "Final check and unblock after Connman restart..."
        sleep 1
        /usr/sbin/rfkill unblock wifi
        
        # Inregistreaza starea finala
        log_message "Final rfkill list:"
        /usr/sbin/rfkill list >> $LOG_FILE 2>&1

        log_message "--- rfkill Unblock Service Finished ---"
        ;;
    *)
        echo "Usage: $0 {start|stop|status}" >&2
        exit 1
esac

exit 0
