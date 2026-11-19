# Rory's eyes (camera board)

Firmware for Rory's camera board, the **Waveshare ESP32-S3-CAM-OV3660**
(37 × 37 mm, with a battery socket), which sits in Rory's body behind her chest.
(The first test board, NULLLAB "ESP32S3 CAM V1.2", still works: see `my_settings.h`.)

It joins your Wi-Fi and makes one photo whenever Rory asks, when he says
*"Rory, look!"* or *"What's this?"*. Her **antenna glows** softly cyan, and
shines bright while she takes the photo, so he can see her looking. The test
page at `http://rory-eyes.local/` shows what it sees.

Rory finds it by the name `rory-eyes` on your Wi-Fi. If she can't (some
routers block those names), add its address to `rory/config.txt` on her SD
card, e.g. `eyes = 192.168.1.250`.

**Antenna LED wiring** (DIYables RGB module → the board's 4-pin **UART** socket,
using a GH1.25-to-Dupont cable): **−** → GND, **R** → TXD, **G and B** → RXD
(both on one wire, for cyan), 3V3 not used. While connecting to Wi-Fi it blinks red.

**Battery:** plug a 3.7 V LiPo into the **BAT** socket (red wire on **+**).
Switch the board on and off with its **PWR** button (the pinhole in Rory's chest).

## Try it

1. In VS Code: **File → Open Folder…** → choose this **`camera`** folder.
2. Click **Build** (✓) once. It creates **`src/my_settings.h`** — open it,
   fill in your Wi-Fi name and password (the same 2.4 GHz network Rory uses)
   and save. Git ignores this file, so updates never overwrite it.
3. Plug in the camera board with USB-C and click **Upload**.
   *If it won't upload:* hold **BOOT**, tap **RESET**, let go of BOOT, try again.
4. Open the **Serial Monitor**. You should see:
   ```
   [eyes] helper chip OK
   [eyes] camera found on the Waveshare ESP32-S3-CAM-OV3660, sensor id 0x3660
   [eyes] ready! open  http://rory-eyes.local/  or  http://192.168.x.x/  in a browser
   ```
5. Open that address on your phone or computer (on the same Wi-Fi) — you'll
   see Rory's view, refreshing every second.

## Privacy

The board only makes a photo when one is asked for, sends it, and forgets it.
It records nothing and stores nothing. Rory will only ask for a photo when
he asks her to look.
