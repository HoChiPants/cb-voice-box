# CB Voice Box

CB Voice Box turns a wired CB microphone and Raspberry Pi Pico interface into private, local speech-to-text. It lives in the macOS menu bar or Windows system tray—there is no main window to manage.

## What it does

- Finds the `CB Voice Box` USB radio automatically at launch and after reconnecting it.
- Records only while the physical push-to-talk button is held.
- Transcribes locally; recordings are not uploaded.
- Copies every result to the clipboard.
- Can paste automatically into the active app.
- Keeps the latest 20 transcriptions in the tray menu.
- Includes Whisper, the English speech model, and every required runtime component inside the app.

## Download

Open the repository's **Actions** tab, choose the latest successful **Build desktop apps** run, and download either:

- `CB-Voice-Box-macOS`
- `CB-Voice-Box-Windows`

The macOS build is not Apple-notarized yet. After trying to open it once, go to **System Settings → Privacy & Security**, scroll to **Security**, and click **Open Anyway** for CB Voice Box. Then approve microphone access. Automatic paste also needs Accessibility permission in **System Settings → Privacy & Security → Accessibility**. This one-time override is Apple's supported method for an app you trust that has not been notarized.

The Windows build is produced automatically but is currently an early, untested build. Windows SmartScreen may warn because the executable is not code-signed.

## Use

1. Flash the Pico with [the current PTT/audio firmware](firmware/cb-voice-box/cb_voice_box_clock_corrected_ptt.uf2).
2. Launch CB Voice Box. A microphone icon appears in the menu bar/system tray.
3. Plug in the radio. The menu changes to **Radio connected** automatically.
4. Hold the physical PTT button. The app listens directly to the CB microphone while PTT is down; release it to transcribe and copy the result.
5. From the tray menu, enable **Paste automatically after listening**, copy the latest result, or select an older result from **History**.

There is no Start Listening button, no separate Whisper installation, no model download, and no audio-device setup. The app opens the CB Voice Box input directly without changing the computer's normal microphone or speaker selection. When PTT is released, it simply stops collecting CB audio; the system microphone remains exactly as it was.

App settings, history, and diagnostics stay in the `CB Voice Box` application-data folder, available through **Open app folder** in the tray menu.

## Hardware

See [Wiring and parts](docs/WIRING.md) for the wiring diagram, pin table, safety notes, and an Amazon parts list with a placeholder Associates tag.

Signal path:

`CB microphone → preamp → PCM1808 ADC → Raspberry Pi Pico → USB → Mac/Windows → local Whisper → clipboard`

## Run from source

Python 3.11 is recommended. Build `whisper-cli` from [whisper.cpp](https://github.com/ggml-org/whisper.cpp), place it beside `cb_voice_box.py`, then:

```bash
python -m venv .venv
source .venv/bin/activate          # Windows: .venv\Scripts\activate
pip install -r requirements.txt
python cb_voice_box.py
```

## Build and privacy notes

- Both desktop downloads are built in public GitHub Actions from this source.
- The speech engine and model are bundled in each desktop download, so the app works offline immediately after installation.
- History contains transcript text, so use **Open app folder** to review or delete `history.json` on a shared computer.
- Amazon product links in the hardware guide use a placeholder tag. Replace it only with an approved Amazon Associates ID and retain a clear affiliate disclosure.

## License

MIT. Third-party components and downloaded models retain their own licenses.
