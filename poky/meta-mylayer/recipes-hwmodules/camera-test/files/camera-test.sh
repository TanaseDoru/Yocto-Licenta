#!/bin/sh
# Test rapid camera RPi Rev 1.3

DEVICE="/dev/video0"
OUTPUT="/tmp/test_capture.jpg"

echo "=== Camera Test RPi Rev 1.3 ==="

# Verificare device
if [ ! -e "$DEVICE" ]; then
    echo "[ERROR] Camera device $DEVICE nu exista!"
    echo "Verifica: start_x=1 in config.txt si modulul bcm2835-v4l2"
    exit 1
fi

# Listare formate suportate
echo "[INFO] Formate suportate:"
v4l2-ctl --device=$DEVICE --list-formats-ext

# Captura imagine raw (JPEG daca e suportat)
echo "[INFO] Capturare imagine -> $OUTPUT"
v4l2-ctl --device=$DEVICE \
    --set-fmt-video=width=1280,height=720,pixelformat=JPEG \
    --stream-mmap \
    --stream-to=$OUTPUT \
    --stream-count=1

if [ $? -eq 0 ]; then
    echo "[SUCCESS] Imaginea a fost capturata: $OUTPUT"
    ls -lh $OUTPUT
else
    echo "[ERROR] Captura a esuat."
    exit 1
fi