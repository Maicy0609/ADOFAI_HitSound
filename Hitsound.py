import json
import re
import sys
import os
import numpy as np
import scipy.io.wavfile as wav
from scipy.signal import resample
import time
import struct


def print_progress(iteration, total, prefix=''):
    percent = int(100 * iteration / total)
    bar = '█' * percent + '░' * (100 - percent)
    sys.stdout.write(f'\r{prefix} {bar} {percent}%')
    sys.stdout.flush()
    if iteration == total:
        print()


class Tile:
    __slots__ = (
        "angle", "bpm", "stdbpm", "bpmangle", "twirl",
        "pause", "midspin", "hold", "clock_wise",
        "offset", "beat", "volume"
    )

    def __init__(self, angle=0.0):
        self.angle = angle
        self.bpm = None
        self.stdbpm = None
        self.bpmangle = 0
        self.twirl = False
        self.pause = 0
        self.midspin = False
        self.hold = False
        self.clock_wise = 1
        self.offset = 0.0
        self.beat = 0.0
        self.volume = -1

    def update(self, prevtile, pitch_factor):
        if prevtile is not None:
            if self.angle == 999:
                self.midspin = True
                self.angle = prevtile.angle - 180

            da = 180 - self.angle + prevtile.angle
            if da >= 360:
                da -= 360
            elif da < 0:
                da += 360
            deltaangle = da

            self.clock_wise = prevtile.clock_wise ^ self.twirl

            if self.clock_wise:
                angleoffset = 360 if (deltaangle == 0 and not self.midspin) else deltaangle
            else:
                angleoffset = 0 if self.midspin else (360 - deltaangle)

            if self.stdbpm is None:
                self.stdbpm = prevtile.stdbpm
            elif self.stdbpm < 0:
                self.stdbpm *= -prevtile.stdbpm

            if self.bpmangle > 0 and angleoffset > 0:
                self.bpm = (
                    self.stdbpm * (angleoffset - self.bpmangle)
                    + prevtile.stdbpm * self.bpmangle
                ) / angleoffset
            else:
                self.bpm = self.stdbpm

            deltabeat = angleoffset * (1.0 / 180.0) + self.pause
            inv_bpm = 60.0 / self.bpm

            self.offset = prevtile.offset + deltabeat * inv_bpm * pitch_factor
            self.beat = prevtile.beat + deltabeat

            if self.volume < 0:
                self.volume = prevtile.volume
        else:
            self.stdbpm = self.stdbpm or 100
            self.bpm = self.bpm or self.stdbpm
            self.clock_wise = 1 ^ self.twirl
            self.offset = 0.0
            self.beat = 0.0
            self.volume = max(self.volume, 100)


def load_adofai(path, pitch=100):
    start_time = time.perf_counter()

    with open(path, "r", encoding="utf-8-sig") as f:
        content = re.sub(r',(\s*[}\]])', r'\1', f.read())
    level = json.loads(content)

    path_data_dict = {
        'R': 0, 'p': 15, 'J': 30, 'E': 45, 'T': 60, 'o': 75,
        'U': 90, 'q': 105, 'G': 120, 'Q': 135, 'H': 150,
        'W': 165, 'L': 180, 'x': 195, 'N': 210, 'Z': 225,
        'F': 240, 'V': 255, 'D': 270, 'Y': 285, 'B': 300,
        'C': 315, 'M': 330, 'A': 345, '5': 555, '6': 666,
        '7': 777, '8': 888, '!': 999
    }

    angledata = level.get("angleData") or [
        path_data_dict.get(c, 0) for c in level.get("pathData", "")
    ]

    n_tiles = len(angledata) + 1
    tiles = [None] * n_tiles

    tiles[0] = Tile()
    tiles[0].stdbpm = level["settings"]["bpm"]
    tiles[0].volume = level["settings"].get("volume", 100)

    print("\n读取谱面数据...")
    for i, angle in enumerate(angledata, 1):
        tiles[i] = Tile(angle)
        if i % max(1, (n_tiles - 1) // 20) == 0:
            print_progress(i, n_tiles - 1, '读取进度:')
    print_progress(n_tiles - 1, n_tiles - 1, '读取进度:')

    actions = level.get("actions", [])
    total_actions = len(actions)

    print("\n处理事件...")
    for i, action in enumerate(actions, 1):
        floor = action.get("floor", -1)
        if 0 <= floor < n_tiles - 1:
            tile = tiles[floor + 1]
            et = action.get("eventType", "")
            if et == "SetSpeed":
                tile.stdbpm = action["beatsPerMinute"] if action.get("speedType") == "Bpm" else -action["bpmMultiplier"]
                tile.bpmangle = action.get("angleOffset", 0)
            elif et == "Twirl":
                tile.twirl = True
            elif et == "Pause":
                tile.pause = action["duration"]
            elif et == "Hold":
                tile.hold = True
                tile.pause += action["duration"] * 2
            elif et == "SetHitsound":
                tile.volume = action["hitsoundVolume"]

        if total_actions > 0 and i % max(1, total_actions // 20) == 0:
            print_progress(i, total_actions, '事件进度:')
    if total_actions > 0:
        print_progress(total_actions, total_actions, '事件进度:')

    print("\n计算时间轴...")
    for i in range(1, n_tiles):
        tiles[i].update(tiles[i - 1], 1.0)
        if i % max(1, n_tiles // 20) == 0:
            print_progress(i, n_tiles, '计算进度:')
    print_progress(n_tiles, n_tiles, '计算进度:')

    print(f"\n谱面加载完成，用时 {time.perf_counter() - start_time:.3f} 秒")
    return tiles


def generate_hitsound(tiles, output_path, pitch=100):
    start_time = time.perf_counter()

    hit_sr, beat = wav.read(os.path.join(os.path.dirname(__file__), "hit.wav"))
    if beat.ndim == 2:
        beat = beat.mean(axis=1)
    beat = beat.astype(np.float32)
    peak = np.abs(beat).max()
    if peak > 0:
        beat /= peak

    if pitch != 100:
        beat = resample(beat, int(len(beat) * 100 / pitch)).astype(np.float32)

    offsets = np.array([t.offset for t in tiles[1:]], dtype=np.float64)
    volumes = np.array([t.volume / 100.0 for t in tiles[1:]], dtype=np.float32)
    pins = (offsets * hit_sr).astype(np.int64)

    L = len(beat)
    total_samples = int(pins[-1] + L)
    output = np.zeros(total_samples, dtype=np.float32)

    n = len(pins)

    print("\n合成 WAV...")
    for i, start in enumerate(pins, 1):
        vol = volumes[i - 1]
        end = start + L
        if end <= total_samples:
            if vol == 1.0:
                output[start:end] += beat
            else:
                output[start:end] += beat * vol
        else:
            tail = total_samples - start
            if tail > 0:
                if vol == 1.0:
                    output[start:] += beat[:tail]
                else:
                    output[start:] += beat[:tail] * vol

        if i % max(1, n // 20) == 0:
            print_progress(i, n, '合成进度:')
    print_progress(n, n, '合成进度:')

    peak = np.abs(output).max()
    if peak > 1.0:
        output /= peak

    wav.write(output_path, hit_sr, (output * 32767).astype(np.int16))
    print(f"\n合成完成，用时 {time.perf_counter() - start_time:.3f} 秒")
    return True


def main():
    adofai_path = input("请输入 .adofai 文件路径: ").strip().strip('"')
    pitch = int(input("请输入音高(默认100): ") or 100)

    tiles = load_adofai(adofai_path, pitch)
    output_path = f"{adofai_path[:-7]}_hitsound.wav"
    generate_hitsound(tiles, output_path, pitch)
    print("\n完成:", output_path)


if __name__ == "__main__":
    main()
