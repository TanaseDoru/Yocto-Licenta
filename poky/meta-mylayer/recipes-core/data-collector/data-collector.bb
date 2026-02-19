SUMMARY = "A data collector from all the sensors and sender"
DESCRIPTION = "Collecting data from all the sensors and sending them via OpenVPN to web server"
LICENSE = "CLOSED"

SRC_URI = "file://data-collector.c \
           file://data-collector.h \
           file://init-script \
           file://update-server-url.sh \
          "

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "data-collector"
INITSCRIPT_PARAMS = "defaults 99"

DEPENDS = "curl"
# RDEPENDS:${PN} = "curl openvpn-client"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -o data-collector data-collector.c -lcurl -lpthread
}

do_install() {
    # Instalare binar
    install -d ${D}${bindir}
    install -m 0755 data-collector ${D}${bindir}/
    
    # Instalare script de inițializare
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/init-script ${D}${sysconfdir}/init.d/data-collector
    
    # Instalare director de configurare
    install -d ${D}${sysconfdir}/data-collector
    
    # URL server prin tunel VPN (10.8.0.1 = laptop-ul prin VPN)
    # Nginx ascultă pe port 80
    echo "http://10.8.0.1/data" > ${D}${sysconfdir}/data-collector/server.conf
    
    # Script pentru actualizare URL (dacă e nevoie mai târziu)
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/update-server-url.sh ${D}${bindir}/update-server-url
}

FILES:${PN} += "${sysconfdir}/init.d/data-collector \
                ${sysconfdir}/data-collector/server.conf \
                ${bindir}/update-server-url \
               "