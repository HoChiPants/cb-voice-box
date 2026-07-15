# CB Voice Box firmware — microphone enumeration test

This is the first Pico SDK/TinyUSB firmware image for the RP2040-Zero. It exposes one USB composite device named **CB Voice Box**:

- a USB Audio Class microphone at 16 kHz, 16-bit (four channels), and
- a HID keyboard that holds `Ctrl + Alt + Shift + P` while GPIO 2 is grounded by the CB microphone PTT switch.

## Flash it

1. Unplug the RP2040-Zero.
2. Hold its **BOOT** button while plugging it back into the Mac.
3. A drive named `RPI-RP2` appears in Finder.
4. Copy [cb_voice_box.uf2](build/cb_voice_box.uf2) onto that drive. The board restarts automatically.
5. Open **System Settings → Sound → Input**. You should now see **CB Voice Box**.
6. In the desktop CB Voice Box app, click **Refresh** and select this input. Confirm that the PTT switch still begins and ends recording.

This image drives the PCM1808 clocks and streams its left I²S input channel to macOS. If the Mac still sees the device but the audio is silent, distorted, or extremely quiet, the USB side is already proven and we will diagnose only the preamp/ADC signal path.

## PCM1808 settings already implied by the photographed board

`FMT`, `MD1`, and `MD0` are unconnected, so the PCM1808's internal pull-downs select **I²S, slave mode**. The final capture firmware will drive the connected pins as follows:

| PCM1808 pin | RP2040 GPIO | Direction |
| --- | ---: | --- |
| BCK | 10 | Pico → PCM1808 |
| LRC | 11 | Pico → PCM1808 |
| DOUT | 12 | PCM1808 → Pico |
| SCK | 13 | Pico → PCM1808 |

No wiring changes are needed for this enumeration test.
