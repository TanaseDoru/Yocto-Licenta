SUMMARY = "Forced RFKill Unblock Service for SysVinit with Debugging"
DESCRIPTION = "Runs rfkill unblock wifi late in boot sequence and logs status."
LICENSE = "CLOSED"

# Setăm clasa de SysVinit ȘI clasa de QA features_check (pentru Yocto Scarthgap)
inherit update-rc.d 

# Scriptul de inițializare
SRC_URI = "file://rfkill-unblock-debug.sh"

# Setăm prioritatea la 99 (printre ultimele care rulează)
INITSCRIPT_NAME = "rfkill-unblock"
INITSCRIPT_PARAMS = "start 99 S ."

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sysconfdir}/init.d
    # Instalăm scriptul cu permisiuni de execuție
    install -m 0755 ${WORKDIR}/rfkill-unblock-debug.sh ${D}${sysconfdir}/init.d/${INITSCRIPT_NAME}
}

# Specificăm fișierele care trebuie incluse în pachet
FILES:${PN} += "${sysconfdir}/init.d/${INITSCRIPT_NAME} /var/log/"

# Asigură-te că rfkill este instalat înainte ca acest script să încerce să îl folosească
RDEPENDS:${PN} += "rfkill"
