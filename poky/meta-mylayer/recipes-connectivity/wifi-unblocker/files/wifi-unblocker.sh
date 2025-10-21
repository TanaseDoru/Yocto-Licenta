# Calea de plasare: recipes-app/rfkill-watcher/wifi-unblocker.sh

#!/bin/sh
# Script de shell pentru deblocarea continua a Wi-Fi-ului

# Asteptare initiala pentru ca majoritatea serviciilor sa porneasca
INITIAL_WAIT=20
CHECK_INTERVAL=5
RFKILL_BIN="/usr/sbin/rfkill"

echo "WiFi Unblocker: Asteptare initiala de $INITIAL_WAIT secunde inainte de inceperea buclei."
sleep $INITIAL_WAIT

while true; do
    # 1. Fortam deblocarea Wi-Fi-ului
    $RFKILL_BIN unblock wifi

    # 2. Logare (optional, pentru depanare)
    # Deoarece acest script ruleaza continuu, logarea ar trebui facuta prin logger
    # logger -t wifi-unblocker "Tentativa de deblocare executata."
    
    # 3. Asteptam 5 secunde inainte de urmatoarea incercare
    sleep $CHECK_INTERVAL
done

# Deoarece este o bucla infinita, acest punct nu ar trebui atins.
exit 0