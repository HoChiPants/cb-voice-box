#!/usr/bin/env python3
"""CB Voice Box: a tiny cross-platform tray transcription app."""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import asdict, dataclass
from pathlib import Path

import numpy as np
import pyperclip
import pystray
import sounddevice as sd
import soundfile as sf
from PIL import Image, ImageDraw
from pynput.keyboard import Controller, Key

SAMPLE_RATE = 16_000
CHANNELS = 4
MIC_CHANNEL = 0
PTT_CHANNEL = 1
PTT_THRESHOLD = 0.05
MIN_RECORDING_SECONDS = 0.25
DEVICE_NAME = "CB Voice Box"
HISTORY_LIMIT = 20


def data_dir() -> Path:
    if sys.platform == "win32":
        root = Path(os.environ.get("LOCALAPPDATA", Path.home()))
    elif sys.platform == "darwin":
        root = Path.home() / "Library" / "Application Support"
    else:
        root = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local" / "share"))
    return root / "CB Voice Box"


DATA_DIR = data_dir()
SETTINGS_PATH = DATA_DIR / "settings.json"
HISTORY_PATH = DATA_DIR / "history.json"
RUNTIME_LOG_PATH = DATA_DIR / "runtime.log"


@dataclass
class Settings:
    auto_paste: bool = False
    whisper_bin: str = ""
    whisper_model: str = ""


def read_json(path: Path, fallback):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (FileNotFoundError, OSError, ValueError):
        return fallback


def write_json(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def clean_transcript(stdout: str) -> str:
    timestamped = re.findall(r"^\s*\[[^]]+\]\s*(.+?)\s*$", stdout, re.MULTILINE)
    if timestamped:
        return " ".join(timestamped).strip()
    ignored = ("whisper_", "system_info:", "main:", "processing ", "load_")
    return " ".join(
        line.strip() for line in stdout.splitlines()
        if line.strip() and not line.strip().lower().startswith(ignored)
    ).strip()


def make_icon() -> Image.Image:
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((7, 7, 57, 57), radius=15, fill="#171717")
    draw.rounded_rectangle((25, 15, 39, 39), radius=7, fill="#f4f4f5")
    draw.arc((19, 23, 45, 48), 0, 180, fill="#f4f4f5", width=4)
    draw.line((32, 48, 32, 54), fill="#f4f4f5", width=4)
    return image


class VoiceBox:
    def __init__(self) -> None:
        raw = read_json(SETTINGS_PATH, {})
        self.settings = Settings(**{k: v for k, v in raw.items() if k in Settings.__annotations__})
        self.history: list[dict] = read_json(HISTORY_PATH, [])[:HISTORY_LIMIT]
        self.stream: sd.InputStream | None = None
        self.device_index: int | None = None
        self.chunks: list[np.ndarray] = []
        self.recording = False
        self.ptt_down = False
        self.running = True
        self.lock = threading.Lock()
        self.status = "Looking for radio…"
        self.icon = pystray.Icon(DEVICE_NAME, make_icon(), DEVICE_NAME, menu=self.menu())

    def log(self, message: str) -> None:
        try:
            DATA_DIR.mkdir(parents=True, exist_ok=True)
            with RUNTIME_LOG_PATH.open("a", encoding="utf-8") as log:
                log.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {message}\n")
        except OSError:
            pass

    def menu(self) -> pystray.Menu:
        history_items = [
            pystray.MenuItem(
                item["text"][:72] + ("…" if len(item["text"]) > 72 else ""),
                self.copy_action(item["text"]),
            )
            for item in self.history[:10]
        ] or [pystray.MenuItem("No transcriptions yet", None, enabled=False)]
        latest = self.history[0]["text"] if self.history else "No transcription yet"
        return pystray.Menu(
            pystray.MenuItem(lambda item: self.status, None, enabled=False),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("Paste automatically after listening", self.toggle_auto_paste, checked=lambda item: self.settings.auto_paste),
            pystray.MenuItem("Copy latest transcription", lambda icon, item: self.copy_text(latest), enabled=bool(self.history)),
            pystray.MenuItem("History", pystray.Menu(*history_items)),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("Reconnect radio", lambda icon, item: self.reconnect()),
            pystray.MenuItem("Open app folder", lambda icon, item: self.open_folder()),
            pystray.MenuItem("Quit", self.quit),
        )

    def copy_action(self, text: str):
        """Create a pystray callback with exactly the required two arguments."""
        def action(_icon, _item) -> None:
            self.copy_text(text)
        return action

    def refresh_menu(self) -> None:
        self.icon.menu = self.menu()
        self.icon.update_menu()

    def set_status(self, status: str, notify: bool = False) -> None:
        self.status = status
        self.log(status)
        self.refresh_menu()
        if notify:
            try:
                self.icon.notify(status, DEVICE_NAME)
            except Exception:
                pass

    def toggle_auto_paste(self, _icon, _item) -> None:
        self.settings.auto_paste = not self.settings.auto_paste
        write_json(SETTINGS_PATH, asdict(self.settings))
        self.refresh_menu()

    def copy_text(self, text: str) -> None:
        pyperclip.copy(text)
        self.set_status("Copied latest transcription")

    def paste(self) -> None:
        keyboard = Controller()
        modifier = Key.cmd if sys.platform == "darwin" else Key.ctrl
        with keyboard.pressed(modifier):
            keyboard.press("v")
            keyboard.release("v")

    def open_folder(self) -> None:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        if sys.platform == "darwin":
            subprocess.Popen(["open", str(DATA_DIR)])
        elif sys.platform == "win32":
            os.startfile(DATA_DIR)  # type: ignore[attr-defined]
        else:
            subprocess.Popen(["xdg-open", str(DATA_DIR)])

    def find_radio(self) -> int | None:
        try:
            sd._terminate()
            sd._initialize()
            for index, device in enumerate(sd.query_devices()):
                if DEVICE_NAME.lower() in device["name"].lower() and device["max_input_channels"] >= CHANNELS:
                    return index
        except Exception as error:
            self.log(f"device scan failed: {error}")
        return None

    def reconnect(self) -> None:
        self.close_stream()
        self.set_status("Looking for radio…")

    def watcher(self) -> None:
        while self.running:
            if self.stream is None:
                device = self.find_radio()
                if device is not None:
                    self.connect(device)
            time.sleep(2)

    def connect(self, device: int) -> None:
        try:
            self.stream = sd.InputStream(
                samplerate=SAMPLE_RATE, channels=CHANNELS, dtype="float32",
                device=device, callback=self.audio_callback,
            )
            self.stream.start()
            self.device_index = device
            self.set_status("Radio connected — hold PTT to talk", notify=True)
        except Exception as error:
            self.stream = None
            self.set_status(f"Radio connection failed: {error}")

    def close_stream(self) -> None:
        stream, self.stream = self.stream, None
        if stream:
            try:
                stream.stop()
                stream.close()
            except Exception:
                pass

    def audio_callback(self, indata, frames, time_info, status) -> None:
        if status:
            self.log(f"audio warning: {status}")
        marker = float(np.sqrt(np.mean(indata[:, PTT_CHANNEL].astype(np.float64) ** 2))) > PTT_THRESHOLD
        finished = None
        with self.lock:
            if marker and not self.ptt_down:
                self.ptt_down = self.recording = True
                self.chunks = []
                self.set_status("Listening…")
            if self.recording:
                self.chunks.append(indata[:, MIC_CHANNEL:MIC_CHANNEL + 1].copy())
            if not marker and self.ptt_down:
                self.ptt_down = self.recording = False
                finished, self.chunks = self.chunks, []
        if finished:
            threading.Thread(target=self.transcribe, args=(finished,), daemon=True).start()

    def speech_paths(self) -> tuple[Path, Path]:
        packaged = Path(getattr(sys, "_MEIPASS", Path(__file__).parent))
        binary = Path(self.settings.whisper_bin) if self.settings.whisper_bin else packaged / ("whisper-cli.exe" if sys.platform == "win32" else "whisper-cli")
        model = Path(self.settings.whisper_model) if self.settings.whisper_model else packaged / "ggml-tiny.en.bin"
        return binary, model

    def transcribe(self, chunks: list[np.ndarray]) -> None:
        audio = np.concatenate(chunks, axis=0)
        if len(audio) / SAMPLE_RATE < MIN_RECORDING_SECONDS:
            self.set_status("Too short — hold PTT a little longer")
            return
        binary, model = self.speech_paths()
        if not binary.is_file() or not model.is_file():
            self.set_status("Speech files missing — reinstall the app", notify=True)
            return
        peak = float(np.max(np.abs(audio)))
        if peak:
            audio *= min(10.0, 0.9 / peak)
        self.set_status("Transcribing locally…")
        with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as wav:
            wav_path = Path(wav.name)
        try:
            sf.write(wav_path, audio, SAMPLE_RATE)
            result = subprocess.run([str(binary), "-m", str(model), "-f", str(wav_path), "-nt", "-np"], capture_output=True, text=True, check=False)
            if result.returncode:
                raise RuntimeError(result.stderr.strip() or "speech engine failed")
            text = clean_transcript(result.stdout)
            if not text:
                raise RuntimeError("no speech was detected")
            pyperclip.copy(text)
            self.history.insert(0, {"time": time.strftime("%Y-%m-%dT%H:%M:%S"), "text": text})
            self.history = self.history[:HISTORY_LIMIT]
            write_json(HISTORY_PATH, self.history)
            self.refresh_menu()
            self.set_status("Transcription copied", notify=True)
            if self.settings.auto_paste:
                time.sleep(0.15)
                self.paste()
        except Exception as error:
            self.set_status(f"Transcription error: {error}", notify=True)
        finally:
            wav_path.unlink(missing_ok=True)

    def quit(self, _icon=None, _item=None) -> None:
        self.running = False
        self.close_stream()
        self.icon.stop()

    def run(self) -> None:
        threading.Thread(target=self.watcher, daemon=True).start()
        self.icon.run()


if __name__ == "__main__":
    VoiceBox().run()
