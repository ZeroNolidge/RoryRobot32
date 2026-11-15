# Rory's eyes (camera board)

Firmware for the **ESP32S3 CAM V1.2** board with the **OV3660** camera — the
board that will sit in Rory's body so she can look at what he shows her.

Right now it's a **camera test**: it joins your Wi-Fi and shows what the
camera sees on a web page. Later Rory's main board fetches a single photo from
it when he says *"Rory, look!"*.

## Try it

1. In VS Code: **File → Open Folder…** → choose this **`camera`** folder.
2. Open **`src/settings.h`** and fill in your Wi-Fi name and password
   (the same 2.4 GHz network Rory uses). Save.
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
