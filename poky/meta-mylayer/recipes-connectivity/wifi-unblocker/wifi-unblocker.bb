# Calea de plasare: recipes-app/rfkill-watcher.bb

SUMMARY = "Daemon de deblocare continua RFKill"
DESCRIPTION = "Ruleaza in fundal pentru a forta deblocarea persistenta a interfetei Wi-Fi."
LICENSE = "CLOSED"

inherit update-rc.d

INITSCRIPT_NAME = "wifi-unblocker-init"
# Prioritate foarte mare pentru a rula DUPĂ ce totul s-a așezat
INITSCRIPT_PARAMS = "start 95 3 5 ." 

# Renuntam la codul C. Acum includem scriptul de shell si initscript-ul.
SRC_URI = "file://wifi-unblocker.sh \
           file://wifi-unblocker-init"

S = "${WORKDIR}"

# RDEPENDS: Acestea trebuie să fie instalate pentru ca scriptul să funcționeze
RDEPENDS:${PN} += "rfkill initscripts" 

do_install() {
    # 1. Instalarea scriptului de buclă în /usr/bin/
    install -d ${D}/usr/bin/
    install -m 0755 ${WORKDIR}/wifi-unblocker.sh ${D}/usr/bin/wifi-unblocker.sh
    
    # 2. Instalarea scriptului SysVinit în /etc/init.d/
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/wifi-unblocker-init ${D}${sysconfdir}/init.d/
}

FILES:${PN} += "/usr/bin/wifi-unblocker.sh ${sysconfdir}/init.d/wifi-unblocker-init"