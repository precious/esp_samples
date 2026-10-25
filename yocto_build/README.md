1. Install Host Prerequisites

Ensure your Linux host machine has the required development packages installed:
bash
sudo apt update
sudo apt install gawk wget git diffstat unzip texinfo gcc build-essential \
chrpath socat cpio python3 python3-pip python3-pexpect xz-utils \
debianutils iputils-ping python3-git python3-jinja2 python3-subunit \
zstd liblz4-tool file locales libsdl1.2-dev xterm
Use code with caution.

2. Clone the Required Repositories

Create a directory for your Yocto project and clone poky alongside the meta-raspberrypi Board Support Package (BSP) layer.
bash
# Create and enter your project workspace
mkdir -p rpi5-yocto && cd rpi5-yocto

# Clone Poky (using the scarthgap branch for Pi 5 compatibility)
git clone -b scarthgap git://git.yoctoproject.org/poky.git

# Navigate into poky and clone the raspberrypi BSP layer
cd poky
git clone -b scarthgap git://git.yoctoproject.org/meta-raspberrypi
Use code with caution.

3. Initialize the Build Environment

Run the environment setup script to create your build directory and open the BitBake shell environment:
bash
source oe-init-build-env
Use code with caution.
Note: This automatically drops you into a newly created build/ directory.

4. Configure Your Layers

Add the meta-raspberrypi layer to your build configuration:
bash
bitbake-layers add-layer ../meta-raspberrypi/
Use code with caution.

5. Modify conf/local.conf

Open conf/local.conf in your preferred text editor and make the following critical adjustments:
bitbake
# Set the machine target for Raspberry Pi 5
MACHINE = "raspberrypi5"

# Use systemd as the init manager (highly recommended for modern kernels)
INIT_MANAGER = "systemd"

# Accept license flags required for the broadcom firmware binary blobs (WiFi/Bluetooth)
LICENSE_FLAGS_ACCEPTED = "synaptics-killswitch"

# (Optional) Optimize for SD card image output formatting
IMAGE_FSTYPES = "wic.bz2 wic.bmap"
Use code with caution.

6. Build the Minimal Image

Run BitBake to build core-image-minimal, which generates a bare-bones console image with just enough packages to boot the hardware:
bash
bitbake core-image-minimal
Use code with caution.
Warning: The first build will download and compile everything from scratch. Depending on your internet speed and CPU, this can take anywhere from 1 to several hours.


Lines to add to config:

```
###
# Set the machine target for Raspberry Pi 5
MACHINE = "raspberrypi5"

# Use systemd as the init manager (highly recommended for modern kernels)
INIT_MANAGER = "systemd"

# Accept license flags required for the broadcom firmware binary blobs (WiFi/Bluetooth)
LICENSE_FLAGS_ACCEPTED = "synaptics-killswitch"

# (Optional) Optimize for SD card image output formatting
IMAGE_FSTYPES = "wic.bz2 wic.bmap"

FETCHCMD_wget = "wget -t 2 -T 30 --passive-ftp --no-check-certificate"
#BB_NUMBER_THREADS = "4"
#PARALLEL_MAKE = "-j 4"
###
```