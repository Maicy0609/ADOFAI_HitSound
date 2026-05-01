#!/usr/bin/env python3
# HitSound.py — Python 重写，等价 MSVC2022 C++17 原版
# 依赖: numpy
# 用法: python HitSound.py  或 双击运行

import os
import sys
import json
import re
import wave
import struct
import numpy as np
from pathlib import Path

# ── Tile 类 ─────────────────────────────────────────────────
class Tile:
    def __init__(self, angle=0.0):
        self.angle = angle
        self.bpm = -1.0
        self.stdbpm = -1.0
        self.bpmangle = 0.0
        self.pause = 0.0
        self.offset = 0.0
        self.beat = 0.0
        self.volume = -1.0
        self.twirl = False
        self.midspin = False
        self.hold = False
        self.cw = 1

    def update(self, prev):
        """根据前一个 Tile 更新当前 Tile 的运动状态"""
        if prev is not None:
            if self.angle == 999.0:
                self.midspin = True
                self.angle = prev.angle - 180.0

            da = 180.0 - self.angle + prev.angle
            if da >= 360.0:
                da -= 360.0
            elif da < 0.0:
                da += 360.0

            self.cw = prev.cw ^ (1 if self.twirl else 0)

            if self.cw:
                ao = 360.0 if (da == 0.0 and not self.midspin) else da
            else:
                ao = 0.0 if self.midspin else (360.0 - da)

            if self.stdbpm < 0.0 and prev.stdbpm > 0.0:
                self.stdbpm = -self.stdbpm * prev.stdbpm
            elif self.stdbpm < 0.0:
                self.stdbpm = prev.stdbpm

            if self.bpmangle > 0.0 and ao > 0.0:
                self.bpm = (self.stdbpm * (ao - self.bpmangle) + prev.stdbpm * self.bpmangle) / ao
            else:
                self.bpm = self.stdbpm

            self.offset = prev.offset + (ao / 180.0 + self.pause) * (60.0 / self.bpm)
            self.beat = prev.beat + ao / 180.0 + self.pause

            if self.volume < 0.0:
                self.volume = prev.volume
        else:
            # 第一个 Tile
            if self.stdbpm < 0.0:
                self.stdbpm = 100.0
            if self.bpm < 0.0:
                self.bpm = self.stdbpm
            self.cw = 1 ^ (1 if self.twirl else 0)
            self.offset = 0.0
            self.beat = 0.0
            self.volume = max(self.volume, 100.0)


# ── 谱面加载 ─────────────────────────────────────────────────
def load_adofai(filepath: str) -> list:
    """解析 .adofai 谱面文件，返回 Tile 列表"""
    with open(filepath, 'rb') as f:
        raw = f.read()

    # 去除 BOM (UTF-8 BOM: EF BB BF)
    if raw[:3] == b'\xef\xbb\xbf':
        raw = raw[3:]

    text = raw.decode('utf-8', errors='replace')

    # 删除控制字符，但保留 \t \n \r
    # C++: remove_if(c < 0x20 && c != '\t' && c != '\n' && c != '\r')
    cleaned = []
    for ch in text:
        if ord(ch) < 0x20 and ch not in ('\t', '\n', '\r'):
            continue
        cleaned.append(ch)
    text = ''.join(cleaned)

    # 去除 JSON 尾随逗号（rapidjson 支持，这里手动处理）
    text = re.sub(r',\s*([}\]])', r'\1', text)

    doc = json.loads(text)

    # 角度映射表
    PD = {
        'R': 0,   'p': 15,  'J': 30,  'E': 45,  'T': 60,  'o': 75,
        'U': 90,  'q': 105, 'G': 120, 'Q': 135, 'H': 150, 'W': 165,
        'L': 180, 'x': 195, 'N': 210, 'Z': 225, 'F': 240, 'V': 255,
        'D': 270, 'Y': 285, 'B': 300, 'C': 315, 'M': 330, 'A': 345,
        '5': 555, '6': 666, '7': 777, '8': 888, '!': 999
    }

    ad = []
    if 'angleData' in doc and isinstance(doc['angleData'], list):
        ad = [float(v) for v in doc['angleData']]
    elif 'pathData' in doc and isinstance(doc['pathData'], str):
        for ch in doc['pathData']:
            ad.append(PD.get(ch, 0.0))

    n = len(ad) + 1
    tiles = [Tile() for _ in range(n)]

    # 读取 settings
    if 'settings' in doc:
        st = doc['settings']
        tiles[0].stdbpm = float(st.get('bpm', 100))
        tiles[0].volume = float(st.get('volume', 100))

    # 初始化从 index 1 开始的 Tile
    for i in range(1, n):
        tiles[i] = Tile(ad[i - 1])

    # 处理 actions
    if 'actions' in doc and isinstance(doc['actions'], list):
        for act in doc['actions']:
            if 'floor' not in act or 'eventType' not in act:
                continue
            fl = int(act['floor'])
            if fl < 0 or fl >= n - 1:
                continue
            t = tiles[fl + 1]
            et = act['eventType']
            if et == 'SetSpeed':
                if 'speedType' in act and act['speedType'] == 'Bpm':
                    t.stdbpm = float(act['beatsPerMinute'])
                elif 'bpmMultiplier' in act:
                    t.stdbpm = -float(act['bpmMultiplier'])
                if 'angleOffset' in act:
                    t.bpmangle = float(act['angleOffset'])
            elif et == 'Twirl':
                t.twirl = True
            elif et == 'Pause':
                if 'duration' in act:
                    t.pause = float(act['duration'])
            elif et == 'Hold':
                t.hold = True
                if 'duration' in act:
                    t.pause += float(act['duration']) * 2
            elif et == 'SetHitsound':
                if 'hitsoundVolume' in act:
                    t.volume = float(act['hitsoundVolume'])

    # 更新所有 Tile（从 1 开始）
    for i in range(1, n):
        tiles[i].update(tiles[i - 1])

    print(f"loaded {n} tiles")
    return tiles


# ── 合成（奈奎斯特过滤 + 静态等功率预缩放）────────────────
def generate(tiles: list, out_path: str):
    """将打击音合成并输出 WAV"""
    # 获取 hit.wav 路径（与可执行文件/脚本同目录）
    exe_dir = Path(sys.executable if getattr(sys, 'frozen', False) else __file__).resolve().parent
    wav_path = exe_dir / 'hit.wav'

    if not wav_path.exists():
        print(f"cannot find {wav_path}")
        sys.exit(1)

    # 读取 hit.wav
    with wave.open(str(wav_path), 'rb') as wf:
        nch = wf.getnchannels()
        sr = wf.getframerate()
        sampwidth = wf.getsampwidth()
        if sampwidth != 2:
            print("only 16-bit PCM supported")
            sys.exit(1)
        frames = wf.readframes(wf.getnframes())
        pcm = np.frombuffer(frames, dtype=np.int16)

    # int16 -> float, 去 DC, 混单声道
    if nch == 2:
        # reshape to (samples, 2) and average
        pcm = pcm.reshape(-1, 2).mean(axis=1)
    beat = pcm.astype(np.float64) / 32768.0
    beat -= beat.mean()   # 去直流

    L = len(beat)  # 单个 hit 的采样数

    # 步骤 1：奈奎斯特过滤（丢弃间隔 < 2/sr 的 hit）
    min_interval = 2.0 / sr
    last_offset = -1e100
    filtered_out = 0

    pins = []   # int64 采样点位置
    vols = []   # 音量系数 (0~1)

    for i in range(1, len(tiles)):
        offset = tiles[i].offset
        if offset - last_offset < min_interval:
            filtered_out += 1
            continue
        last_offset = offset
        pins.append(int(round(offset * sr)))
        vols.append(float(tiles[i].volume / 100.0))

    if not pins:
        print("all hits filtered out; no output")
        return

    print(f"after Nyquist filter: {len(pins)} hits ({filtered_out} removed)")

    # 步骤 2：计算最大同时发声密度
    total_len = pins[-1] + L
    diff = np.zeros(total_len + 1, dtype=np.int64)
    for p in pins:
        beg = p
        end = min(p + L, total_len)
        diff[beg] += 1
        diff[end] -= 1

    max_density = int(np.cumsum(diff)[:total_len].max())
    print(f"max simultaneous hits (after filter): {max_density}")

    if max_density == 0:
        print("no overlapping hits")
        return

    # 步骤 3：静态等功率预缩放
    pre_scale = 1.0 / np.sqrt(float(max_density))
    beat_scaled = beat * pre_scale

    # 混合（double 精度累加）
    buf = np.zeros(total_len, dtype=np.float64)
    pins = np.array(pins, dtype=np.int64)
    vols = np.array(vols, dtype=np.float64)

    for idx, p in enumerate(pins):
        v = vols[idx]
        cnt = min(L, total_len - p)
        buf[p:p + cnt] += beat_scaled[:cnt] * v
        if (idx + 1) % max(1, len(pins) // 50) == 0:
            percent = (idx + 1) * 100 // len(pins)
            print(f"\rmixing {percent}%", end='', flush=True)
    print()

    # 峰值与安全写入
    peak = float(np.abs(buf).max())
    safety_gain = 0.98
    final_scale = safety_gain
    if peak * final_scale > 1.0:
        final_scale = 1.0 / peak

    # 转换为 int16，限幅
    out = buf * final_scale
    out = np.clip(out, -1.0, 1.0)
    out_int16 = (out * 32767.0).astype(np.int16)

    # 写入 WAV
    with wave.open(out_path, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)   # 16-bit
        wf.setframerate(sr)
        wf.writeframes(out_int16.tobytes())

    print(f"done: {out_path}  (peak={peak:.4f})")


# ── main ─────────────────────────────────────────────────────
def main():
    if sys.platform == 'win32':
        # 设置控制台编码为 UTF-8（避免中文乱码）
        try:
            import ctypes
            ctypes.windll.kernel32.SetConsoleOutputCP(65001)
            ctypes.windll.kernel32.SetConsoleCP(65001)
        except:
            pass

    path = input("adofai path: ").strip()
    # 去除两端可能存在的引号
    if path and path[0] == '"':
        path = path[1:]
    if path and path[-1] == '"':
        path = path[:-1]

    if not os.path.isfile(path):
        print(f"file not found: {path}")
        return

    tiles = load_adofai(path)
    out_path = os.path.splitext(path)[0] + '.wav'
    generate(tiles, out_path)

    input("press enter...")


if __name__ == '__main__':
    main()