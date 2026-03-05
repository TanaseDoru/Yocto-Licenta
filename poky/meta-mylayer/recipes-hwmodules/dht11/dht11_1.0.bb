SUMMARY     = "DHT11 temperature and humidity sensor client for data-collector"
DESCRIPTION = "Citeste DHT11 via sysfs GPIO si trimite valori prin Unix socket catre data-collector."
LICENSE     = "CLOSED"

SRC_URI = " \
    file://dht11.h          \
    file://dht11.c          \
    file://dht11-client.c   \
    file://dht11.conf       \
    file://dht11-client.init \
"

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME   = "dht11-client"
INITSCRIPT_PARAMS = "defaults 91 10"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        -O2 -Wall -Wextra      \
        -I${S}                 \
        ${S}/dht11.c ${S}/dht11-client.c \
        -lrt                   \
        -o ${S}/dht11-client
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${S}/dht11-client ${D}${bindir}/dht11-client

    install -d ${D}${sysconfdir}
    install -m 0644 ${S}/dht11.conf ${D}${sysconfdir}/dht11.conf

    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${S}/dht11-client.init \
        ${D}${sysconfdir}/init.d/dht11-client
}

FILES:${PN} = " \
    ${bindir}/dht11-client          \
    ${sysconfdir}/dht11.conf        \
    ${sysconfdir}/init.d/dht11-client \
"

# Fara RDEPENDS pe pigpio