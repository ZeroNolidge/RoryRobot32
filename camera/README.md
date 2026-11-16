# Rory's eyes (camera board)

Firmware for the **ESP32S3 CAM V1.2** board with the **OV3660** camera — the
board that will sit in Rory's body so she can look at what he shows her.

It joins your Wi-Fi and makes one photo whenever Rory asks — when he says
*"Rory, look!"* or *"What's this?"*. Its two white LEDs blink softly while it
takes the photo, so he can see her looking. The test page at
`http://rory-eyes.local/` shows what it sees.

Rory finds it by the name `rory-eyes` on your Wi-Fi. If she can't (some
routers block those names), add its address to `rory/config.txt` on her SD
card, e.g. `eyes = 192.168.1.250`.

## Try it

1. In VS Code: **File → Open Folder…** → choose this **`camera`** folder.
2. Click **Build** (✓) once. It creates **`src/my_settings.h`** — open it,
   fill in your Wi-Fi name and password (the same 2.4 GHz network Rory uses)
   and save. Git ignores this file, so updates never overwrite it.
3. Plug in the camera board with USB-C and click **Upload**.
   *If it won't upload:* hold **BOOT**, tap **RESET**, let go of BOOT, try again.
4. Open the **Serial Monitor**. You should see:
   ```
   [eyes] camera found: ESP32-S3-EYE wiring, sensor id 0x3660
   [eyes] ready! open  http://rory-eyes.local/  or  http://192.168.x.x/  in a browser
   ```
5. Open that address on your phone or computer (on the same Wi-Fi) — you'll
   see Rory's view, refreshing every second.

## Privacy

The board only makes a photo when one is asked for, sends it, and forgets it.
It records nothing and stores nothing. Rory will only ask for a photo when
he asks her to look.
