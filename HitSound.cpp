#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <xmmintrin.h>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"


struct Tile {
    double angle;
    double bpm;
    double stdbpm;
    double bpmangle;
    bool twirl;
    double pause;
    bool midspin;
    bool hold;
    bool clock_wise;
    double offset;
    double beat;
    double volume;

    Tile(double a = 0.0)
        : angle(a), bpm(0.0), stdbpm(0.0), bpmangle(0.0), twirl(false), pause(0.0),
        midspin(false), hold(false), clock_wise(true), offset(0.0), beat(0.0), volume(-1.0) {
    }
};

static void Tile_update(Tile& tile, const Tile* prev, double pitch_factor) {
    if (prev == nullptr) {
        tile.stdbpm = tile.stdbpm > 0.0 ? tile.stdbpm : 100.0;
        tile.bpm = tile.bpm > 0.0 ? tile.bpm : tile.stdbpm;
        tile.clock_wise = !tile.twirl;
        tile.offset = 0.0;
        tile.beat = 0.0;
        tile.volume = tile.volume >= 0.0 ? tile.volume : 100.0;
        return;
    }

    if (tile.angle == 999.0) {
        tile.midspin = true;
        tile.angle = prev->angle - 180.0;
    }

    double da = 180.0 - tile.angle + prev->angle;
    if (da >= 360.0) da -= 360.0;
    else if (da < 0.0) da += 360.0;
    double deltaangle = da;

    tile.clock_wise = prev->clock_wise != tile.twirl;

    double angleoffset = tile.clock_wise
        ? ((deltaangle == 0.0 && !tile.midspin) ? 360.0 : deltaangle)
        : (tile.midspin ? 0.0 : (360.0 - deltaangle));

    if (tile.stdbpm == 0.0) tile.stdbpm = prev->stdbpm;
    else if (tile.stdbpm < 0.0) tile.stdbpm *= -prev->stdbpm;

    if (tile.bpmangle > 0.0 && angleoffset > 0.0) {
        tile.bpm = (tile.stdbpm * (angleoffset - tile.bpmangle) + prev->stdbpm * tile.bpmangle) / angleoffset;
    }
    else {
        tile.bpm = tile.stdbpm;
    }

    double deltabeat = angleoffset / 180.0 + tile.pause;
    double inv_bpm = 60.0 / tile.bpm;
    tile.offset = prev->offset + deltabeat * inv_bpm * pitch_factor;
    tile.beat = prev->beat + deltabeat;

    if (tile.volume < 0.0) tile.volume = prev->volume;
}

static std::string get_exe_directory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    PathRemoveFileSpecA(path);
    return std::string(path);
}

static std::pair<int, std::vector<float>> read_wav(const std::string& path) {
    FILE* fp = nullptr;
    fopen_s(&fp, path.c_str(), "rb");
    if (!fp) throw std::runtime_error("Cannot open hit.wav");

    char riff[4], wave[4];
    uint32_t fsize;
    fread(riff, 1, 4, fp);
    fread(&fsize, 4, 1, fp);
    fread(wave, 1, 4, fp);

    int sample_rate = 0, channels = 0;
    std::vector<float> audio;

    while (true) {
        char chunk_id[4] = { 0 };
        uint32_t chunk_size = 0;
        if (fread(chunk_id, 1, 4, fp) != 4) break;
        fread(&chunk_size, 4, 1, fp);

        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            uint16_t format, chan;
            uint32_t sr;
            uint16_t bps;
            fread(&format, 2, 1, fp);
            fread(&chan, 2, 1, fp);
            channels = chan;
            fread(&sr, 4, 1, fp);
            sample_rate = static_cast<int>(sr);
            fseek(fp, 6, SEEK_CUR);
            fread(&bps, 2, 1, fp);
            if (chunk_size > 16) fseek(fp, static_cast<long>(chunk_size) - 16, SEEK_CUR);
        }
        else if (strncmp(chunk_id, "data", 4) == 0) {
            std::vector<int16_t> raw(chunk_size / 2);
            fread(raw.data(), 2, raw.size(), fp);

            audio.resize(raw.size() / channels);
            if (channels == 1) {
                for (size_t i = 0; i < audio.size(); ++i) {
                    audio[i] = raw[i] / 32768.0f;
                }
            }
            else if (channels == 2) {
                for (size_t i = 0; i < audio.size(); ++i) {
                    float l = raw[2 * i] / 32768.0f;
                    float r = raw[2 * i + 1] / 32768.0f;
                    audio[i] = (l + r) * 0.5f;
                }
            }
            break;
        }
        else {
            fseek(fp, static_cast<long>(chunk_size), SEEK_CUR);
        }
    }
    fclose(fp);
    return { sample_rate, audio };
}

static void write_wav(const std::string& path, int sr, const std::vector<int16_t>& data) {
    FILE* fp = nullptr;
    fopen_s(&fp, path.c_str(), "wb");
    if (!fp) throw std::runtime_error("Cannot write output WAV");

    fwrite("RIFF", 1, 4, fp);
    uint32_t filesize = 36 + static_cast<uint32_t>(data.size() * 2);
    fwrite(&filesize, 4, 1, fp);
    fwrite("WAVEfmt ", 1, 8, fp);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, fp);
    uint16_t pcm = 1, chan = 1;
    fwrite(&pcm, 2, 1, fp);
    fwrite(&chan, 2, 1, fp);
    fwrite(&sr, 4, 1, fp);
    uint32_t byte_rate = static_cast<uint32_t>(sr) * 2;
    fwrite(&byte_rate, 4, 1, fp);
    uint16_t block = 2;
    fwrite(&block, 2, 1, fp);
    uint16_t bits = 16;
    fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp);
    uint32_t data_size = static_cast<uint32_t>(data.size() * 2);
    fwrite(&data_size, 4, 1, fp);
    fwrite(data.data(), 2, data.size(), fp);
    fclose(fp);
}

static std::vector<float> pitch_shift(const std::vector<float>& input, double factor) {
    if (factor == 1.0) return input;
    int new_len = static_cast<int>(input.size() / factor);
    std::vector<float> output(new_len);
    for (int i = 0; i < new_len; ++i) {
        double src_idx = i * factor;
        int idx0 = static_cast<int>(src_idx);
        int idx1 = std::min(idx0 + 1, static_cast<int>(input.size()) - 1);
        float frac = static_cast<float>(src_idx - idx0);
        output[i] = input[idx0] * (1.0f - frac) + input[idx1] * frac;
    }
    return output;
}

static void print_progress(size_t cur, size_t total, const std::string& prefix) {
    if (total == 0) return;
    int percent = static_cast<int>(100.0 * cur / total);
    size_t filled = 80 * cur / total;
    std::string bar(80, '-');
    std::fill(bar.begin(), bar.begin() + filled, '#');
    printf("\r%s %s %d%%", prefix.c_str(), bar.c_str(), percent);
    fflush(stdout);
    if (cur == total) printf("\n");
}

static std::vector<Tile> load_adofai(const std::string& path) {
    auto start = std::chrono::high_resolution_clock::now();

    FILE* fp = nullptr;
    fopen_s(&fp, path.c_str(), "rb");
    if (!fp) throw std::runtime_error("Cannot open .adofai");
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::string content(static_cast<size_t>(fsize), '\0');
    fread(&content[0], 1, static_cast<size_t>(fsize), fp);
    fclose(fp);

    size_t start_offset = 0;
    if (content.size() >= 3 && static_cast<uint8_t>(content[0]) == 0xEF &&
        static_cast<uint8_t>(content[1]) == 0xBB && static_cast<uint8_t>(content[2]) == 0xBF) {
        start_offset = 3;
    }

    rapidjson::Document doc;
    doc.ParseInsitu<rapidjson::kParseTrailingCommasFlag>(const_cast<char*>(content.data() + start_offset));
    if (doc.HasParseError()) {
        throw std::runtime_error(rapidjson::GetParseError_En(doc.GetParseError()));
    }

    double init_bpm = doc["settings"]["bpm"].GetDouble();
    double init_volume = doc["settings"].HasMember("volume") ? doc["settings"]["volume"].GetDouble() : 100.0;

    std::unordered_map<char, double> path_map = {
        {'R',0},{'p',15},{'J',30},{'E',45},{'T',60},{'o',75},{'U',90},{'q',105},
        {'G',120},{'Q',135},{'H',150},{'W',165},{'L',180},{'x',195},{'N',210},
        {'Z',225},{'F',240},{'V',255},{'D',270},{'Y',285},{'B',300},{'C',315},
        {'M',330},{'A',345},{'5',555},{'6',666},{'7',777},{'8',888},{'!',999}
    };

    std::vector<double> angles;
    if (doc.HasMember("angleData")) {
        const auto& arr = doc["angleData"];
        for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
            angles.push_back(arr[i].GetDouble());
        }
    }
    else {
        std::string pd = doc["pathData"].GetString();
        for (char c : pd) {
            auto it = path_map.find(c);
            angles.push_back(it != path_map.end() ? it->second : 0.0);
        }
    }

    size_t n_tiles = angles.size() + 1;
    std::vector<Tile> tiles(n_tiles);
    tiles[0].stdbpm = init_bpm;
    tiles[0].volume = init_volume;

    std::cout << "\n读取谱面数据...\n";
    for (size_t i = 0; i < angles.size(); ++i) {
        tiles[i + 1].angle = angles[i];
        if ((i + 1) % std::max<size_t>(1, angles.size() / 20) == 0) {
            print_progress(i + 1, angles.size(), "读取进度:");
        }
    }
    print_progress(angles.size(), angles.size(), "读取进度:");

    std::cout << "处理事件...\n";
    size_t total_actions = doc.HasMember("actions") ? doc["actions"].Size() : 0;
    for (size_t i = 0; i < total_actions; ++i) {
        const auto& act = doc["actions"][static_cast<rapidjson::SizeType>(i)];
        int64_t floor = act.HasMember("floor") ? act["floor"].GetInt64() : -1;
        if (floor >= 0 && static_cast<size_t>(floor + 1) < n_tiles) {
            Tile& t = tiles[static_cast<size_t>(floor) + 1];
            std::string et = act["eventType"].GetString();
            if (et == "SetSpeed") {
                std::string stype = act["speedType"].GetString();
                if (stype == "Bpm") {
                    t.stdbpm = act["beatsPerMinute"].GetDouble();
                }
                else {
                    t.stdbpm = -act["bpmMultiplier"].GetDouble();
                }
                t.bpmangle = act.HasMember("angleOffset") ? act["angleOffset"].GetDouble() : 0.0;
            }
            else if (et == "Twirl") t.twirl = true;
            else if (et == "Pause") t.pause = act["duration"].GetDouble();
            else if (et == "Hold") {
                t.hold = true;
                t.pause += act["duration"].GetDouble() * 2.0;
            }
            else if (et == "SetHitsound") t.volume = act["hitsoundVolume"].GetDouble();
        }
        if ((i + 1) % std::max<size_t>(1, total_actions / 20) == 0) {
            print_progress(i + 1, total_actions, "事件进度:");
        }
    }
    if (total_actions > 0) print_progress(total_actions, total_actions, "事件进度:");

    std::cout << "计算时间轴...\n";
    Tile_update(tiles[0], nullptr, 1.0);
    for (size_t i = 1; i < n_tiles; ++i) {
        Tile_update(tiles[i], &tiles[i - 1], 1.0);
        if (i % std::max<size_t>(1, n_tiles / 20) == 0) {
            print_progress(i, n_tiles, "计算进度:");
        }
    }
    print_progress(n_tiles, n_tiles, "计算进度:");

    auto dur = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
    printf("\n谱面加载完成，用时 %.3f 秒\n", dur);

    return tiles;
}

static void generate_hitsound(const std::vector<Tile>& tiles, const std::string& out_path, int pitch) {
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

    auto start = std::chrono::high_resolution_clock::now();

    std::string hit_path = get_exe_directory() + "\\hit.wav";
    auto [hit_sr, original_beat] = read_wav(hit_path);

    float peak = 0.0f;
    for (float v : original_beat) peak = std::max(peak, std::abs(v));
    if (peak > 0.0f) {
        for (float& v : original_beat) v /= peak;
    }

    int base_pitch = (pitch <= 37) ? 25 : (pitch <= 75) ? 50 : (pitch <= 150) ? 100 : 200;
    double shift_factor = 100.0 / base_pitch;
    std::vector<float> hit_beat = pitch_shift(original_beat, shift_factor);

    

    size_t n = tiles.size() - 1;
    std::vector<double> offsets(n);
    std::vector<float> volumes(n);
    for (size_t i = 0; i < n; ++i) {
        offsets[i] = tiles[i + 1].offset;
        volumes[i] = static_cast<float>(tiles[i + 1].volume / 100.0);
    }

    std::vector<int64_t> pins(n);
    for (size_t i = 0; i < n; ++i) {
        pins[i] = static_cast<int64_t>(offsets[i] * hit_sr);  // 恢复原始写法
    }

    size_t L = hit_beat.size();
    int64_t total64 = pins.back() + static_cast<int64_t>(L);
    size_t total_samples = static_cast<size_t>(std::max<int64_t>(total64, 0));
    std::vector<float> output(total_samples, 0.0f);

    std::cout << "合成 WAV...\n";
    for (size_t i = 0; i < n; ++i) {
        int64_t start_pos = pins[i];
        if (start_pos < 0) continue;
        float vol = volumes[i];
        size_t len = std::min(L, total_samples - static_cast<size_t>(start_pos));
        for (size_t j = 0; j < len; ++j) {
            output[static_cast<size_t>(start_pos) + j] += hit_beat[j] * vol;
        }
        if ((i + 1) % std::max<size_t>(1, n / 20) == 0) {
            print_progress(i + 1, n, "合成进度:");
        }
    }
    print_progress(n, n, "合成进度:");

    peak = 0.0f;
    for (float v : output) peak = std::max(peak, std::abs(v));
    if (peak > 1.0f) {
        for (float& v : output) v /= peak;
    }

    std::vector<int16_t> out16(total_samples);
    for (size_t i = 0; i < total_samples; ++i) {
        float v = output[i] * 32767.0f;
        if (v >= 32767.0f) out16[i] = 32767;
        else if (v <= -32768.0f) out16[i] = -32768;
        else out16[i] = static_cast<int16_t>(v + (v >= 0.0f ? 0.5f : -0.5f));
    }

    write_wav(out_path, hit_sr, out16);

    auto dur = std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
    printf("\n合成完成，用时 %.3f 秒\n", dur);
}

int main() {
    std::string path;
    std::cout << "请输入 .adofai 文件路径: ";
    std::getline(std::cin, path);
    if (!path.empty() && path.front() == '"') path.erase(0, 1);
    if (!path.empty() && path.back() == '"') path.pop_back();

    int pitch = 100;
    std::string pitch_str;
    std::cout << "请输入音高(默认100): ";
    std::getline(std::cin, pitch_str);
    if (!pitch_str.empty()) pitch = std::stoi(pitch_str);

    auto tiles = load_adofai(path);

    size_t dot = path.find_last_of('.');
    std::string base = (dot != std::string::npos) ? path.substr(0, dot) : path;
    std::string out_path = base + "_p" + std::to_string(pitch) + ".wav";

    generate_hitsound(tiles, out_path, pitch);

    std::cout << "\n完成: " << out_path << "\n";
    std::cout << "Press Enter to exit...";
    std::getchar();
    return 0;
}
