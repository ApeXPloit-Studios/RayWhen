
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Audio Packer GUI: Convert audio to raw .r8 (U8 PCM) and .r1 (1-bit DPCM) with zero headers.
# - No external Python deps.
# - WAV/AIFF handled via stdlib (wave, aifc).
# - If 'ffmpeg' is on PATH, any format (mp3, flac, ogg, etc.) is supported by decoding through ffmpeg to PCM WAV.
#
# Spec recap:
# .r8 : unsigned 8-bit PCM, mono, fixed sample rate (default 11025 Hz), no header
# .r1 : 1-bit DPCM stream (MSB-first) where each bit indicates delta +1 (1) or -1 (0) applied to an 8-bit accumulator.
#       Encoder targets the 8-bit waveform; per sample, it pushes the accumulator one step toward the target value.
import io
import os
import sys
import struct
import math
import random
import shutil
import tempfile
import subprocess
import threading
import traceback

import tkinter as tk
from tkinter import ttk, filedialog, messagebox

import wave
try:
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        import aifc
    AIFC_AVAILABLE = True
except ImportError:
    AIFC_AVAILABLE = False

APP_TITLE = "Audio Packer (.r8 / .r1)"
DEFAULT_RATE = 11025

# ----------------------------- Utility: ffmpeg presence -----------------------------

def have_ffmpeg():
    return shutil.which("ffmpeg") is not None

def decode_with_ffmpeg(src_path, tmp_dir):
    """Use ffmpeg to decode ANY input to a temporary 32-bit float mono WAV at source rate.
    Returns path to WAV or raises Exception.
    """
    # We'll decode to 32-bit float mono at the *original* sample rate (ffmpeg auto-selects)
    # then we'll resample ourselves.
    out_wav = os.path.join(tmp_dir, "decoded.wav")
    cmd = [
        "ffmpeg", "-y",
        "-i", src_path,
        "-ac", "1",           # mono
        "-c:a", "pcm_f32le",  # 32-bit float
        out_wav
    ]
    try:
        completed = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    except Exception as e:
        raise RuntimeError(f"ffmpeg decode failed: {e}\\n{getattr(e, 'stderr', b'').decode(errors='ignore')}")
    return out_wav

# ----------------------------- WAV/AIFF readers (PCM only) -----------------------------

def read_pcm_wav(path):
    """Read a PCM WAV using stdlib. Return (samples_float_mono, sample_rate)."""
    with wave.open(path, 'rb') as wf:
        nch = wf.getnchannels()
        fr = wf.getframerate()
        sw = wf.getsampwidth()
        nframes = wf.getnframes()
        raw = wf.readframes(nframes)
    # Convert raw PCM to float32 [-1,1]
    if sw == 1:
        # 8-bit unsigned
        data = [(b - 128) / 128.0 for b in raw]
        # interleaved if nch>1
    elif sw == 2:
        # 16-bit signed little-endian
        it = struct.iter_unpack('<h', raw)
        data = [s[0] / 32768.0 for s in it]
    elif sw == 3:
        # 24-bit signed little-endian
        # Read triplets and sign-extend
        data = []
        for i in range(0, len(raw), 3):
            b0, b1, b2 = raw[i], raw[i+1], raw[i+2]
            val = b0 | (b1 << 8) | (b2 << 16)
            if val & 0x800000:
                val -= 1 << 24
            data.append(val / 8388608.0)
    elif sw == 4:
        # 32-bit signed or float? WAV stdlib often returns raw PCM, assume signed int32
        # Try both: first assume int32, but detect non-integer if remainder frames/4 mismatch?
        # We'll try int32, and if it clips weirdly user should route via ffmpeg path.
        it = struct.iter_unpack('<i', raw)
        data = [s[0] / 2147483648.0 for s in it]
    else:
        raise ValueError(f"Unsupported WAV sample width: {sw*8} bits")
    # Downmix to mono if needed
    if nch > 1:
        mono = []
        for i in range(0, len(data), nch):
            s = 0.0
            for c in range(nch):
                s += data[i+c]
            mono.append(s / nch)
        data = mono
    return data, fr

def read_pcm_aiff(path):
    """Read AIFF/AIFF-C (PCM) using stdlib. Return (samples_float_mono, sample_rate)."""
    if not AIFC_AVAILABLE:
        raise RuntimeError("AIFF support not available (aifc module not found)")
    with aifc.open(path, 'rb') as af:
        nch = af.getnchannels()
        fr = af.getframerate()
        sw = af.getsampwidth()
        nframes = af.getnframes()
        raw = af.readframes(nframes)
    # AIFF is big-endian for integer PCM
    if sw == 1:
        data = [(b - 128) / 128.0 for b in raw]
    elif sw == 2:
        it = struct.iter_unpack('>h', raw)
        data = [s[0] / 32768.0 for s in it]
    elif sw == 3:
        data = []
        for i in range(0, len(raw), 3):
            b0, b1, b2 = raw[i], raw[i+1], raw[i+2]
            val = (b0 << 16) | (b1 << 8) | b2
            if val & 0x800000:
                val -= 1 << 24
            data.append(val / 8388608.0)
    elif sw == 4:
        it = struct.iter_unpack('>i', raw)
        data = [s[0] / 2147483648.0 for s in it]
    else:
        raise ValueError(f"Unsupported AIFF sample width: {sw*8} bits")
    # Downmix
    if nch > 1:
        mono = []
        for i in range(0, len(data), nch):
            s = 0.0
            for c in range(nch):
                s += data[i+c]
            mono.append(s / nch)
        data = mono
    return data, fr

# ----------------------------- DSP helpers -----------------------------

def normalize(samples):
    """Normalize to peak 0.99."""
    if not samples:
        return samples
    peak = max(abs(x) for x in samples)
    if peak < 1e-9:
        return samples[:]
    gain = 0.99 / peak
    return [max(-1.0, min(1.0, x * gain)) for x in samples]

def tpdf_dither(samples, scale=1.0/256.0):
    """Add very light TPDF dither before quantization to 8-bit."""
    out = []
    for x in samples:
        # Two independent uniform rands in [-0.5,0.5], summed => triangular PDF in [-1,1]
        n = (random.random() - 0.5) + (random.random() - 0.5)
        out.append(x + n * scale)
    return out

def resample_linear(samples, src_rate, dst_rate):
    if src_rate == dst_rate or len(samples) < 2:
        return samples[:]
    ratio = dst_rate / float(src_rate)
    out_len = int(math.floor(len(samples) * ratio))
    if out_len <= 1:
        return [samples[0]]
    out = [0.0] * out_len
    for i in range(out_len):
        t = i / ratio
        i0 = int(math.floor(t))
        i1 = min(i0 + 1, len(samples)-1)
        frac = t - i0
        out[i] = samples[i0] * (1.0 - frac) + samples[i1] * frac
    return out

def float_to_u8(samples):
    # clamp to [-1,1], map to [0,255]
    out = bytearray(len(samples))
    for i, x in enumerate(samples):
        if x < -1.0: x = -1.0
        if x >  1.0: x =  1.0
        u = int(round((x * 0.5 + 0.5) * 255.0))
        if u < 0: u = 0
        if u > 255: u = 255
        out[i] = u
    return bytes(out)

def encode_r1_from_u8(u8_bytes):
    """Encode 8-bit waveform to 1-bit DPCM stream as described:
    - Start acc=128
    - For each target sample x in [0..255], emit bit=1 if acc < x else 0, then acc += +1 or -1 (clamped).
      Pack bits MSB-first per byte.
    Returns raw bytes of packed bits (no header).
    """
    acc = 128
    bitbuf = 0
    bitcnt = 0
    out = bytearray()
    for target in u8_bytes:
        if acc < target:
            bit = 1
            acc += 1
            if acc > 255: acc = 255
        else:
            bit = 0
            acc -= 1
            if acc < 0: acc = 0
        bitbuf = (bitbuf << 1) | bit
        bitcnt += 1
        if bitcnt == 8:
            out.append(bitbuf & 0xFF)
            bitbuf = 0
            bitcnt = 0
    if bitcnt:
        # pad remaining bits at LSB side
        bitbuf <<= (8 - bitcnt)
        out.append(bitbuf & 0xFF)
    return bytes(out)

# ----------------------------- Core conversion -----------------------------

def load_audio_any(path):
    """Return (samples_float_mono, src_rate). Uses ffmpeg if available; else WAV/AIFF only."""
    ext = os.path.splitext(path)[1].lower()
    tmp_dir = None
    try:
        if ext in ('.wav',):
            return read_pcm_wav(path)
        if ext in ('.aif', '.aiff'):
            if AIFC_AVAILABLE:
                return read_pcm_aiff(path)
            else:
                raise RuntimeError("AIFF files not supported without aifc module. Use ffmpeg or provide WAV files.")
        # Other formats need ffmpeg
        if have_ffmpeg():
            tmp_dir = tempfile.mkdtemp(prefix='apacker_')
            wav_path = decode_with_ffmpeg(path, tmp_dir)
            data, fr = read_pcm_wav(wav_path)
            return data, fr
        else:
            raise RuntimeError("Unsupported format without ffmpeg. Please install ffmpeg or provide WAV/AIFF.")
    finally:
        if tmp_dir:
            try:
                shutil.rmtree(tmp_dir)
            except Exception:
                pass

def convert_to_r8_and_r1(input_path, out_dir, dst_rate=DEFAULT_RATE, make_r8=True, make_r1=True, normalize_on=True, add_dither=True):
    samples, src_rate = load_audio_any(input_path)
    # Normalize & resample
    work = samples
    if normalize_on:
        work = normalize(work)
    work = resample_linear(work, src_rate, dst_rate)
    if add_dither:
        work = tpdf_dither(work, scale=1.0/512.0)  # very light for 8-bit

    # To u8 waveform
    u8 = float_to_u8(work)

    base = os.path.splitext(os.path.basename(input_path))[0]
    outputs = []

    if make_r8:
        r8_path = os.path.join(out_dir, base + ".r8")
        with open(r8_path, "wb") as f:
            f.write(u8)
        outputs.append(r8_path)

    if make_r1:
        r1_bytes = encode_r1_from_u8(u8)
        r1_path = os.path.join(out_dir, base + ".r1")
        with open(r1_path, "wb") as f:
            f.write(r1_bytes)
        outputs.append(r1_path)

    return outputs, dst_rate, len(u8)

# ----------------------------- GUI -----------------------------

class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("720x420")
        self.minsize(660, 380)

        # State
        self.files = []
        self.output_dir = os.getcwd()

        self.create_widgets()

    def create_widgets(self):
        pad = {'padx': 8, 'pady': 6}

        top = ttk.Frame(self)
        top.pack(fill='x', **pad)

        ttk.Label(top, text="Target Rate (Hz):").pack(side='left')
        self.rate_var = tk.StringVar(value=str(DEFAULT_RATE))
        self.rate_entry = ttk.Entry(top, textvariable=self.rate_var, width=10)
        self.rate_entry.pack(side='left', padx=(4,12))

        self.chk_r8 = tk.BooleanVar(value=True)
        self.chk_r1 = tk.BooleanVar(value=True)
        ttk.Checkbutton(top, text="Write .r8", variable=self.chk_r8).pack(side='left')
        ttk.Checkbutton(top, text="Write .r1", variable=self.chk_r1).pack(side='left', padx=(12,0))

        self.chk_norm = tk.BooleanVar(value=True)
        self.chk_dither = tk.BooleanVar(value=True)
        ttk.Checkbutton(top, text="Normalize", variable=self.chk_norm).pack(side='left', padx=(18,0))
        ttk.Checkbutton(top, text="Dither (light)", variable=self.chk_dither).pack(side='left', padx=(12,0))

        if have_ffmpeg():
            ttk.Label(top, text="ffmpeg: available").pack(side='right')
        else:
            ttk.Label(top, text="ffmpeg: NOT found (WAV/AIFF only)").pack(side='right')

        mid = ttk.Frame(self)
        mid.pack(fill='both', expand=True, **pad)

        # File list
        left = ttk.Frame(mid)
        left.pack(side='left', fill='both', expand=True)

        self.listbox = tk.Listbox(left, height=10, selectmode='extended')
        self.listbox.pack(fill='both', expand=True, padx=0, pady=(0,6))

        btns = ttk.Frame(left)
        btns.pack(fill='x')
        ttk.Button(btns, text="Add Files…", command=self.on_add).pack(side='left')
        ttk.Button(btns, text="Remove Selected", command=self.on_remove).pack(side='left', padx=(6,0))
        ttk.Button(btns, text="Clear", command=self.on_clear).pack(side='left', padx=(6,0))

        # Right panel
        right = ttk.Frame(mid)
        right.pack(side='left', fill='y', padx=(12,0))

        outrow = ttk.Frame(right)
        outrow.pack(fill='x', pady=(0,6))
        ttk.Label(outrow, text="Output Dir:").pack(side='left')
        self.out_var = tk.StringVar(value=self.output_dir)
        self.out_entry = ttk.Entry(outrow, textvariable=self.out_var)
        self.out_entry.pack(side='left', fill='x', expand=True, padx=(6,6))
        ttk.Button(outrow, text="Browse…", command=self.on_browse_out).pack(side='left')

        self.log = tk.Text(right, width=44, height=12, wrap='word')
        self.log.pack(fill='both', expand=True)

        # Bottom controls
        bottom = ttk.Frame(self)
        bottom.pack(fill='x', **pad)
        self.progress = ttk.Progressbar(bottom, orient='horizontal', mode='determinate')
        self.progress.pack(fill='x', expand=True, side='left')
        ttk.Button(bottom, text="Convert", command=self.on_convert).pack(side='left', padx=(8,0))

    # ------------------ UI actions ------------------

    def on_add(self):
        types = [
            ("Audio", "*.wav *.aif *.aiff *.mp3 *.flac *.ogg *.m4a *.aac *.wma *.opus *.webm"),
            ("WAV", "*.wav"),
            ("All files", "*.*"),
        ]
        paths = filedialog.askopenfilenames(title="Add audio files", filetypes=types)
        for p in paths:
            if p not in self.files:
                self.files.append(p)
                self.listbox.insert('end', p)

    def on_remove(self):
        sel = list(self.listbox.curselection())
        sel.reverse()
        for idx in sel:
            path = self.listbox.get(idx)
            self.files.remove(path)
            self.listbox.delete(idx)

    def on_clear(self):
        self.files.clear()
        self.listbox.delete(0, 'end')

    def on_browse_out(self):
        d = filedialog.askdirectory(title="Choose output folder", mustexist=True)
        if d:
            self.output_dir = d
            self.out_var.set(d)

    def on_convert(self):
        try:
            rate = int(self.rate_var.get().strip())
            if rate < 2000 or rate > 48000:
                raise ValueError
        except Exception:
            messagebox.showerror(APP_TITLE, "Invalid target rate. Use 2000..48000 Hz.")
            return
        if not (self.chk_r8.get() or self.chk_r1.get()):
            messagebox.showerror(APP_TITLE, "Select at least one output: .r8 or .r1")
            return
        if not self.files:
            messagebox.showerror(APP_TITLE, "Add some input files first.")
            return
        out_dir = self.out_var.get().strip()
        if not out_dir or not os.path.isdir(out_dir):
            messagebox.showerror(APP_TITLE, "Invalid output directory.")
            return

        # Run in background thread so UI doesn't block
        t = threading.Thread(target=self._convert_worker, args=(rate, out_dir, self.chk_r8.get(), self.chk_r1.get(), self.chk_norm.get(), self.chk_dither.get()), daemon=True)
        t.start()

    def _log(self, text):
        self.log.insert('end', text + "\n")
        self.log.see('end')
        self.update_idletasks()

    def _convert_worker(self, rate, out_dir, make_r8, make_r1, norm, dith):
        self.progress['maximum'] = len(self.files)
        self.progress['value'] = 0
        self._log(f"Starting… target {rate} Hz | R8={make_r8} R1={make_r1} | normalize={norm} dither={dith}")
        for i, path in enumerate(self.files, 1):
            try:
                self._log(f"[{i}/{len(self.files)}] {os.path.basename(path)}")
                outs, dst_rate, n = convert_to_r8_and_r1(path, out_dir, dst_rate=rate, make_r8=make_r8, make_r1=make_r1, normalize_on=norm, add_dither=dith)
                szs = ", ".join([f"{os.path.basename(o)} ({os.path.getsize(o)} bytes)" for o in outs])
                self._log(f"  -> {szs}")
            except Exception as e:
                self._log("  !! ERROR: " + str(e))
                self._log(traceback.format_exc())
            self.progress['value'] = i
            self.update_idletasks()
        self._log("Done.")

def main():
    app = App()
    app.mainloop()

if __name__ == "__main__":
    main()
