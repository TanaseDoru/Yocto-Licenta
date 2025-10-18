# Baking a recipe (the image used for rpi)
bitbake core-image-minimal

# Serial Comunication
sudo minicom -s
To check all ttys do
```bash
ls /dev/tty*
```
# Wifi Communication
- Added features like:
  - IMAGE_INSTALL:append = " \
  - linux-firmware-rpidistro-bcm43455" -> Model de kernel pentru interfata de wireless
  - linux-firmware-bcm43430 -> Model de kernel pentru interfata wireless (backward compatibility)
  - iw -> Idk dar tot pentru wifi
  - kernel-modules ->
  - connman
  - connman-client
  - connman-wifi-config
  - wpa-supplicant
