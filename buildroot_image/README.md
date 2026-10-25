# Buildroot 2026.08.1 — working Raspberry Pi 5 Wi-Fi image

This recipe produces a Raspberry Pi 5 image that:

- boots with `systemd`;
- joins a WPA/WPA2-Personal Wi-Fi network automatically on `wlan0`;
- receives an IPv4 address through DHCP;
- starts an SSH server and permits password login for user `seva`;
- includes Python 3.

It deliberately installs the Raspberry Pi-maintained CYW43455 firmware pair. The generic firmware shipped by the selected Buildroot `linux-firmware` package was too old for this setup and led to `Firmware rejected country settings`, channel-setting failures, and Wi-Fi authentication timeouts.

## 1. Start from the Raspberry Pi 5 default configuration

On a Debian/Ubuntu build host, install the standard Buildroot build tools:

```sh
sudo apt-get update
sudo apt-get install -y build-essential bc bzip2 cpio file gawk git \
  libncurses-dev perl python3 rsync unzip wget
```

Download Buildroot and create a separate output directory:

```sh
wget https://buildroot.org/downloads/buildroot-2026.08.1.tar.gz
tar -xf buildroot-2026.08.1.tar.gz
mkdir output
cd output
```

Then run:

```sh
make O=$PWD -C ../buildroot-2026.08.1 raspberrypi5_defconfig
make O=$PWD -C ../buildroot-2026.08.1 menuconfig
```

Unless a step explicitly says otherwise, run `make` commands from the output directory. Other paths are relative to the Buildroot source directory.

## 2. Enable systemd

In `menuconfig`, set:

```text
System configuration
  → Init system
    → systemd
```

Leave the Raspberry Pi 5 bootloader and kernel defaults unchanged.

## 3. Enable packages and DHCP

In `menuconfig`, enable:

```text
Target packages
  → Networking applications
    → wpa_supplicant
      → nl80211 driver support
      → wpa_cli                 # optional but useful for diagnosis
    → wireless-regdb
    → openssh
      → server
      → key utilities

Target packages
  → Hardware handling
    → Firmware
      → linux-firmware
        → Broadcom BRCM bcm43xxx
        → Cypress CY cyw43xxx

Target packages
  → Interpreter languages and scripting
    → python3
```

Also set:

```text
System configuration
  → Network interface to configure through DHCP: wlan0
```

`wireless-regdb` is required so the kernel can load the regulatory database. It does not replace the Raspberry Pi Wi-Fi firmware installed in step 4.

## 4. Create the root filesystem overlay, including the proven Pi 5 firmware

Create this directory structure in the Buildroot source tree:

```text
board/my-rpi5/rootfs-overlay/
├── etc/
│   ├── ssh/sshd_config
│   ├── systemd/network/wlan0.network
│   └── wpa_supplicant/wpa_supplicant-wlan0.conf
└── usr/lib/firmware/
    ├── brcm/
    └── cypress/
```

Important: use `usr/lib/firmware`, **not** `lib/firmware`. Buildroot uses a merged `/usr` filesystem, so an overlay containing a top-level `lib/` directory is rejected.

Set this menu option:

```text
System configuration
  → Root filesystem overlay directories:
    board/my-rpi5/rootfs-overlay
```

Create the directories before adding the files in the following steps:

```sh
mkdir -p board/my-rpi5/rootfs-overlay/etc/{ssh,systemd/network,wpa_supplicant}
mkdir -p board/my-rpi5/rootfs-overlay/usr/lib/firmware/{brcm,cypress}
```

### Install the Raspberry Pi CYW43455 firmware

Run the following commands from the Buildroot source directory. They fetch the current Raspberry Pi-maintained standard firmware, regulatory CLM data, and NVRAM settings. These files override only the Pi 5 Wi-Fi firmware selected by the kernel.

```sh
fw_overlay=board/my-rpi5/rootfs-overlay/usr/lib/firmware
mkdir -p "$fw_overlay/brcm" "$fw_overlay/cypress"

wget -O "$fw_overlay/cypress/cyfmac43455-sdio.bin" \
  https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree/trixie/debian/added-firmware/cypress/cyfmac43455-sdio-standard.bin

wget -O "$fw_overlay/cypress/cyfmac43455-sdio.clm_blob" \
  https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree/trixie/debian/added-firmware/cypress/cyfmac43455-sdio.clm_blob

wget -O "$fw_overlay/brcm/brcmfmac43455-sdio.txt" \
  https://raw.githubusercontent.com/RPi-Distro/firmware-nonfree/trixie/debian/added-firmware/brcm/brcmfmac43455-sdio.txt

ln -sfn ../cypress/cyfmac43455-sdio.bin \
  "$fw_overlay/brcm/brcmfmac43455-sdio.raspberrypi,5-model-b.bin"

ln -sfn ../cypress/cyfmac43455-sdio.clm_blob \
  "$fw_overlay/brcm/brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob"

ln -sfn brcmfmac43455-sdio.txt \
  "$fw_overlay/brcm/brcmfmac43455-sdio.raspberrypi,5-model-b.txt"
```

For the firmware version proven in this image, the downloaded files have these SHA-256 values:

```text
d608f866582519c0a28d86db43040f4f1b98dd1d153e72e9752586546b4a36c3  cyfmac43455-sdio.bin
9823842cae9fb9a5dd1e5fb31f595516ec7deee341354bef30bb3026eee29cc1  cyfmac43455-sdio.clm_blob
ca709be81a78bdb6932936374f39943acbd7af07fae6151011127599a3ce9e3d  brcmfmac43455-sdio.txt
```

The important result is:

```text
usr/lib/firmware/brcm/
├── brcmfmac43455-sdio.txt
├── brcmfmac43455-sdio.raspberrypi,5-model-b.bin -> ../cypress/cyfmac43455-sdio.bin
├── brcmfmac43455-sdio.raspberrypi,5-model-b.clm_blob -> ../cypress/cyfmac43455-sdio.clm_blob
└── brcmfmac43455-sdio.raspberrypi,5-model-b.txt -> brcmfmac43455-sdio.txt
```

## 5. Configure Wi-Fi and systemd-networkd

Create `board/my-rpi5/rootfs-overlay/etc/wpa_supplicant/wpa_supplicant-wlan0.conf`:

```ini
country=UA

network={
    ssid="YOUR_WIFI_SSID"
    psk="YOUR_WIFI_PASSWORD"
}
```

Replace both placeholders. For another country, replace `UA` with its two-letter ISO 3166 country code. Keep this file simple; do not add `ctrl_interface` unless it is specifically needed.

Protect the password:

```sh
chmod 600 board/my-rpi5/rootfs-overlay/etc/wpa_supplicant/wpa_supplicant-wlan0.conf
```

Create `board/my-rpi5/rootfs-overlay/etc/systemd/network/wlan0.network`:

```ini
[Match]
Name=wlan0

[Network]
DHCP=ipv4
```

This explicitly tells `systemd-networkd` to request the IPv4 lease after `wpa_supplicant` associates with the access point.

## 6. Create the `seva` user and configure SSH

Generate a password hash on the build machine:

```sh
openssl passwd -6
```

Create `board/my-rpi5/users.table`:

```text
seva -2 seva -2 YOUR_PASSWORD_HASH /home/seva /bin/sh - Seva
```

Replace `YOUR_PASSWORD_HASH` with the complete output of `openssl passwd -6`.

Set this menu option:

```text
System configuration
  → Path to the users tables:
    board/my-rpi5/users.table
```

Create `board/my-rpi5/rootfs-overlay/etc/ssh/sshd_config`:

```text
Port 22
PermitRootLogin no
PasswordAuthentication yes
KbdInteractiveAuthentication no
UsePAM no
AllowUsers seva
```

## 7. Enable Wi-Fi authentication and SSH at boot

Buildroot runs `systemctl preset-all` while creating the filesystem image. That step can remove a `wpa_supplicant@wlan0.service` link placed directly in the overlay. Create it afterwards with a post-fakeroot script.

Create `board/my-rpi5/post-fakeroot.sh`:

```sh
#!/bin/sh
set -eu

target_dir="$1"
wants_dir="$target_dir/etc/systemd/system/multi-user.target.wants"

mkdir -p "$wants_dir"

ln -sfn /usr/lib/systemd/system/wpa_supplicant@.service \
  "$wants_dir/wpa_supplicant@wlan0.service"

ln -sfn /usr/lib/systemd/system/sshd.service \
  "$wants_dir/sshd.service"
```

Make it executable:

```sh
chmod 755 board/my-rpi5/post-fakeroot.sh
```

Set this menu option:

```text
System configuration
  → Custom scripts to run inside the fakeroot environment:
    board/my-rpi5/post-fakeroot.sh
```

Do not enable `wpa_supplicant@wlan0.service` manually in the overlay or by running `systemctl` in the build directory; the post-fakeroot script is the reliable image-build-time method.

## 8. Set image size, build, and verify

In `menuconfig`, set:

```text
Filesystem images
  → exact size: 512M
```

Save and build from the output directory:

```sh
make O=$PWD -C ../buildroot-2026.08.1
```

Flash `images/sdcard.img` (inside the output directory) to an SD card and boot the Pi.

After boot, these commands confirm that the important parts are working:

```sh
systemctl is-active wpa_supplicant@wlan0.service
ip addr show wlan0
dmesg | grep 'Firmware: BCM4345/6'
```

Expected results:

- `wpa_supplicant@wlan0.service` is `active`;
- `wlan0` has an `inet` IPv4 address;
- the firmware line reports `Version: 7.45.265` or a newer Raspberry Pi firmware version.

You can then connect over SSH:

```sh
ssh seva@PI_IP_ADDRESS
```
