# YKBFirmware

## Build

```bash
# Create NCS Workspace directory
mkdir -p ~/ykb-firmware-ws

# Create python venv + source it
python3 -m venv ~/ykb-firmware-ws/venv
source ~/ykb-firmware-ws/venv/bin/activate

# Install first deps
pip install -U pip wheel
pip install -U west

# Move to NCS dir, really important
cd ~/ncs-ws/ncs

# Initialize project, SDK and deps
west init -m https://github.com/jayadamsmorgan/YKBFirmware --mr master
west update
west zephyr-export

# Install python requirements for Zephyr scripts
pip install -r zephyr/scripts/requirements.txt

# Install toolchains
west sdk install
```
