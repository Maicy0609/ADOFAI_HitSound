// HitSound.cpp — MSVC2022 C++17
// cl /std:c++17 /O2 /arch:AVX2 /EHsc HitSound.cpp /I./rapidjson

#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdint>

#pragma warning(push)
#pragma warning(disable: 4996)
#include "rapidjson/document.h"
#pragma warning(pop)

// ── Tile ─────────────────────────────────────────────────────
struct Tile {
    double angle = 0, bpm = -1, stdbpm = -1, bpmangle = 0, pause = 0, offset = 0, beat = 0, volume = -1;
    bool twirl = false, midspin = false, hold = false;
    int cw = 1;
    explicit Tile(double a = 0) :angle(a) {}

    void update(const Tile* p) {
        if (p) {
            if (angle == 999.0) { midspin = true; angle = p->angle - 180.0; }
            double da = 180.0 - angle + p->angle;
            if (da >= 360)da -= 360; else if (da < 0)da += 360;
            cw = p->cw ^ (twirl ? 1 : 0);
            double ao = cw ? ((da == 0 && !midspin) ? 360 : da) : (midspin ? 0 : (360 - da));
            if (stdbpm < 0 && p->stdbpm>0) stdbpm = -stdbpm * p->stdbpm;
            else if (stdbpm < 0)         stdbpm = p->stdbpm;
            bpm = (bpmangle > 0 && ao > 0) ? (stdbpm * (ao - bpmangle) + p->stdbpm * bpmangle) / ao : stdbpm;
            offset = p->offset + (ao / 180.0 + pause) * (60.0 / bpm);
            beat = p->beat + ao / 180.0 + pause;
            if (volume < 0)volume = p->volume;
        }
        else {
            if (stdbpm < 0)stdbpm = 100; if (bpm < 0)bpm = stdbpm;
            cw = 1 ^ (twirl ? 1 : 0); offset = 0; beat = 0; volume = std::max(volume, 100.0);
        }
    }
};

// ── 谱面加载 ─────────────────────────────────────────────────
static std::vector<Tile> load_adofai(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << path << '\n'; exit(1); }
    std::string s((std::istreambuf_iterator<char>(f)), {});

    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF)
        s.erase(0, 3);
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) {
        return c < 0x20 && c != '\t' && c != '\n' && c != '\r'; }), s.end());

    rapidjson::Document doc;
    doc.Parse<rapidjson::kParseTrailingCommasFlag>(s.c_str());
    if (doc.HasParseError()) { std::cerr << "JSON error\n"; exit(1); }

    static const std::unordered_map<char, double> pd = {
        {'R',0},{'p',15},{'J',30},{'E',45},{'T',60},{'o',75},
        {'U',90},{'q',105},{'G',120},{'Q',135},{'H',150},{'W',165},
        {'L',180},{'x',195},{'N',210},{'Z',225},{'F',240},{'V',255},
        {'D',270},{'Y',285},{'B',300},{'C',315},{'M',330},{'A',345},
        {'5',555},{'6',666},{'7',777},{'8',888},{'!',999} };

    std::vector<double> ad;
    if (doc.HasMember("angleData") && doc["angleData"].IsArray()) {
        const auto& a = doc["angleData"];
        for (rapidjson::SizeType i = 0; i < a.Size(); ++i)ad.push_back(a[i].GetDouble());
    }
    else if (doc.HasMember("pathData") && doc["pathData"].IsString()) {
        for (char c : std::string(doc["pathData"].GetString())) {
            auto it = pd.find(c); ad.push_back(it != pd.end() ? it->second : 0.0);
        }
    }

    size_t n = ad.size() + 1;
    std::vector<Tile> tiles(n);
    if (doc.HasMember("settings")) {
        const auto& st = doc["settings"];
        tiles[0].stdbpm = st.HasMember("bpm") ? st["bpm"].GetDouble() : 100;
        tiles[0].volume = st.HasMember("volume") ? st["volume"].GetDouble() : 100;
    }
    for (size_t i = 1; i < n; ++i)tiles[i] = Tile(ad[i - 1]);

    if (doc.HasMember("actions") && doc["actions"].IsArray()) {
        const auto& acts = doc["actions"];
        for (rapidjson::SizeType i = 0; i < acts.Size(); ++i) {
            const auto& a = acts[i];
            if (!a.HasMember("floor") || !a.HasMember("eventType"))continue;
            int fl = a["floor"].GetInt();
            if (fl < 0 || fl >= (int)n - 1)continue;
            Tile& t = tiles[(size_t)fl + 1];
            std::string et = a["eventType"].GetString();
            if (et == "SetSpeed") {
                if (a.HasMember("speedType") && std::string(a["speedType"].GetString()) == "Bpm")
                    t.stdbpm = a["beatsPerMinute"].GetDouble();
                else if (a.HasMember("bpmMultiplier"))
                    t.stdbpm = -a["bpmMultiplier"].GetDouble();
                if (a.HasMember("angleOffset"))t.bpmangle = a["angleOffset"].GetDouble();
            }
            else if (et == "Twirl") {
                t.twirl = true;
            }
            else if (et == "Pause") {
                if (a.HasMember("duration"))t.pause = a["duration"].GetDouble();
            }
            else if (et == "Hold") {
                t.hold = true; if (a.HasMember("duration"))t.pause += a["duration"].GetDouble() * 2;
            }
            else if (et == "SetHitsound") { if (a.HasMember("hitsoundVolume"))t.volume = a["hitsoundVolume"].GetDouble(); }
        }
    }
    for (size_t i = 1; i < n; ++i)tiles[i].update(&tiles[i - 1]);
    std::cout << "loaded " << n << " tiles\n";
    return tiles;
}

// ── 合成（新方案：奈奎斯特过滤 + 静态等功率预缩放）───────
static void generate(const std::vector<Tile>& tiles, const std::string& out_path)
{
    // hit.wav 路径
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH); PathRemoveFileSpecA(exe);
    std::string wav_path = std::string(exe) + "\\hit.wav";

    FILE* fp = fopen(wav_path.c_str(), "rb");
    if (!fp) { std::cerr << "cannot find " << wav_path << '\n'; exit(1); }
    char tmp4[4]; uint32_t u32;
    (void)fread(tmp4, 1, 4, fp); (void)fread(&u32, 4, 1, fp); (void)fread(tmp4, 1, 4, fp);
    uint16_t nch = 1; uint32_t sr = 44100; std::vector<int16_t> pcm;
    while (true) {
        char id[4]; uint32_t sz = 0;
        if (fread(id, 1, 4, fp) != 4 || fread(&sz, 4, 1, fp) != 1)break;
        if (!strncmp(id, "fmt ", 4)) {
            uint16_t af = 1; uint32_t tr = std::min(sz, 8u);
            if (tr >= 2)(void)fread(&af, 2, 1, fp);
            if (tr >= 4)(void)fread(&nch, 2, 1, fp);
            if (tr >= 8)(void)fread(&sr, 4, 1, fp);
            fseek(fp, (long)sz - (long)tr, SEEK_CUR);
        }
        else if (!strncmp(id, "data", 4)) {
            pcm.resize(sz / 2); (void)fread(pcm.data(), 1, sz, fp); break;
        }
        else fseek(fp, (long)sz, SEEK_CUR);
    }
    fclose(fp);
    if (pcm.empty()) { std::cerr << "no PCM\n"; exit(1); }

    // int16 → float, 去DC, 混单声道
    std::vector<float> beat;
    if (nch == 2) { for (size_t i = 0; i + 1 < pcm.size(); i += 2)beat.push_back(((float)pcm[i] + (float)pcm[i + 1]) * 0.5f / 32768.0f); }
    else { for (auto v : pcm)beat.push_back((float)v / 32768.0f); }
    float dc = 0; for (float v : beat)dc += v; dc /= (float)beat.size();
    for (float& v : beat)v -= dc;

    const size_t L = beat.size();

    // ══════════════════════════════════════════════════════════
    // 步骤 1：奈奎斯特过滤（丢弃间隔 < 1/24000 的 hit）
    // ══════════════════════════════════════════════════════════
    const double min_interval = 2.0 / sr;     // 1 / (sr/2)
    double last_offset = -1e100;
    size_t filtered_out = 0;

    std::vector<int64_t> pins;
    std::vector<float> vols;

    for (size_t i = 1; i < tiles.size(); ++i) {
        double offset = tiles[i].offset;
        if (offset - last_offset < min_interval) {
            ++filtered_out;
            continue;          // 只丢弃，不移动时间戳
        }
        last_offset = offset;
        pins.push_back((int64_t)(offset * (double)sr));
        vols.push_back((float)(tiles[i].volume / 100.0));
    }

    if (pins.empty()) {
        std::cerr << "all hits filtered out; no output\n";
        return;
    }
    std::cout << "after Nyquist filter: " << pins.size()
        << " hits (" << filtered_out << " removed)\n";

    // ══════════════════════════════════════════════════════════
    // 步骤 2：计算最大同时发声密度（差分数组，仅基于保留的 hit）
    // ══════════════════════════════════════════════════════════
    const int64_t total_len = pins.back() + (int64_t)L;
    std::vector<int64_t> diff((size_t)total_len + 1, 0);

    for (size_t i = 0; i < pins.size(); ++i) {
        int64_t beg = pins[i];
        int64_t end = std::min(beg + (int64_t)L, total_len);
        diff[(size_t)beg] += 1;
        diff[(size_t)end] -= 1;
    }

    int64_t cur = 0;
    int64_t max_density = 0;
    for (size_t i = 0; i < (size_t)total_len; ++i) {
        cur += diff[i];
        if (cur > max_density) max_density = cur;
    }
    std::cout << "max simultaneous hits (after filter): " << max_density << "\n";

    if (max_density == 0) {
        std::cerr << "no overlapping hits\n";
        return;
    }

    // ══════════════════════════════════════════════════════════
    // 步骤 3：静态等功率预缩放（整个 hit 波形乘以同一常数）
    // ══════════════════════════════════════════════════════════
    const float pre_scale = 1.0f / std::sqrt((float)max_density);
    std::vector<float> beat_scaled(L);
    for (size_t i = 0; i < L; ++i)
        beat_scaled[i] = beat[i] * pre_scale;

    // 混音（double 累加）
    std::vector<double> buf((size_t)total_len, 0.0);
    for (size_t i = 0; i < pins.size(); ++i) {
        int64_t p = pins[i];
        float v = vols[i];
        int64_t cnt = std::min((int64_t)L, total_len - p);
        for (int64_t j = 0; j < cnt; ++j)
            buf[p + j] += (double)beat_scaled[j] * v;
        if ((i + 1) % std::max<size_t>(1, pins.size() / 50) == 0)
            std::cout << "\rmixing " << (i + 1) * 100 / pins.size() << "%" << std::flush;
    }
    std::cout << "\n";

    // double → float，并找出实际峰值
    std::vector<float> out((size_t)total_len);
    float peak = 0.0f;
    for (size_t i = 0; i < (size_t)total_len; ++i) {
        out[i] = (float)buf[i];
        peak = std::max(peak, std::fabs(out[i]));
    }

    // ══════════════════════════════════════════════════════════
    // 步骤 4：安全写入 int16（使用常数 safety gain + 饱和钳位）
    // 不引入动态处理，只是避免极偶然的+0.1 dB过冲
    // ══════════════════════════════════════════════════════════
    const float safety_gain = 0.98f;   // 留约 0.2 dB 余量
    // 如果实际峰值 × safety_gain > 1.0，则再用峰值归一化（但极少发生）
    float final_scale = safety_gain;
    if (peak * final_scale > 1.0f)
        final_scale = 1.0f / peak;     // 安全兜底，仍为常数

    // 写 WAV
    uint32_t dsz = (uint32_t)std::min((int64_t)total_len * 2, (int64_t)UINT32_MAX);
    uint32_t fsz = 36 + dsz, br = sr * 2, f16 = 16;
    uint16_t af = 1, oc = 1, ba = 2, bp = 16;
    std::ofstream wav(out_path, std::ios::binary);
    wav.write("RIFF", 4); wav.write((char*)&fsz, 4); wav.write("WAVE", 4);
    wav.write("fmt ", 4); wav.write((char*)&f16, 4);
    wav.write((char*)&af, 2); wav.write((char*)&oc, 2); wav.write((char*)&sr, 4);
    wav.write((char*)&br, 4); wav.write((char*)&ba, 2); wav.write((char*)&bp, 2);
    wav.write("data", 4); wav.write((char*)&dsz, 4);

    for (float s : out) {
        int32_t val = (int32_t)(s * final_scale * 32767.0f);
        int16_t x = (int16_t)std::clamp(val, -32768, 32767);   // 饱和，不削波
        wav.write((char*)&x, 2);
    }
    std::cout << "done: " << out_path << "  (peak=" << peak << ")\n";
}

// ══════════════════════════════════════════════════════════════
// ── 响度平衡后处理（动态加载 AudioLoudnorm.dll）────────────
// ══════════════════════════════════════════════════════════════

// 函数指针类型定义（匹配 AudioLoudnorm.h 的实际 API）
struct LoudnormStats {
    double integrated_loudness;
    double loudness_range;
    double true_peak;
    double threshold;
    double offset;
};

typedef void* (*LoudnormCreate)(int, int, double, double, double, int);
typedef int   (*LoudnormProcess)(void*, const float*, int);
typedef int   (*LoudnormGetOutput)(void*, float*, int);
typedef int   (*LoudnormFlush)(void*, float*, int);
typedef int   (*LoudnormGetStats)(void*, LoudnormStats*);
typedef void  (*LoudnormDestroy)(void*);
typedef const char* (*LoudnormVersion)();

// 简易 WAV 数据结构
struct WavData {
    std::vector<float> samples;  // 交错：LRLRLR...
    int sample_rate = 44100;
    int channels = 1;
    int64_t total_frames = 0;
};

static WavData read_wav_float(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) { std::cerr << "cannot open " << path << "\n"; return {}; }

    char tmp4[4]; uint32_t u32;
    fread(tmp4, 1, 4, fp); fread(&u32, 4, 1, fp); fread(tmp4, 1, 4, fp);

    WavData w;
    uint16_t bits = 16;
    while (true) {
        char id[4]; uint32_t sz = 0;
        if (fread(id, 1, 4, fp) != 4 || fread(&sz, 4, 1, fp) != 1) break;
        if (!strncmp(id, "fmt ", 4)) {
            uint16_t af = 1;
            uint32_t tr = std::min(sz, 8u);
            if (tr >= 2) fread(&af, 2, 1, fp);
            if (tr >= 4) fread(&w.channels, 2, 1, fp);
            if (tr >= 8) fread(&w.sample_rate, 4, 1, fp);
            if (tr >= 14)fread(&bits, 2, 1, fp);
            fseek(fp, (long)sz - (long)tr, SEEK_CUR);
        }
        else if (!strncmp(id, "data", 4)) {
            size_t n_samples = sz / (bits / 8);
            w.samples.resize(n_samples);

            if (bits == 16) {
                std::vector<int16_t> pcm(n_samples);
                fread(pcm.data(), 1, sz, fp);
                for (size_t i = 0; i < n_samples; ++i)
                    w.samples[i] = pcm[i] / 32768.0f;
            }
            else if (bits == 32) {
                std::vector<int32_t> pcm(n_samples);
                fread(pcm.data(), 1, sz, fp);
                for (size_t i = 0; i < n_samples; ++i)
                    w.samples[i] = pcm[i] / 2147483648.0f;
            }
            else if (bits == 24) {
                for (size_t i = 0; i < n_samples; ++i) {
                    unsigned char b[3];
                    fread(b, 1, 3, fp);
                    int32_t v = b[0] | (b[1] << 8) | (b[2] << 16);
                    if (v & 0x800000) v |= 0xFF000000;
                    w.samples[i] = v / 8388608.0f;
                }
            }
            break;
        }
        else fseek(fp, (long)sz, SEEK_CUR);
    }
    fclose(fp);
    w.total_frames = w.samples.size() / w.channels;
    return w;
}

static void write_wav_float(const std::string& path, const float* samples,
    int64_t total_frames, int sample_rate, int channels) {
    std::ofstream wav(path, std::ios::binary);
    uint32_t dsz = (uint32_t)(total_frames * channels * 2);
    uint32_t fsz = 36 + dsz, br = sample_rate * channels * 2;
    uint16_t f16 = 16, af = 1, oc = (uint16_t)channels, ba = (uint16_t)(channels * 2), bp = 16;

    wav.write("RIFF", 4); wav.write((char*)&fsz, 4); wav.write("WAVE", 4);
    wav.write("fmt ", 4); wav.write((char*)&f16, 4);
    wav.write((char*)&af, 2); wav.write((char*)&oc, 2);
    wav.write((char*)&sample_rate, 4); wav.write((char*)&br, 4);
    wav.write((char*)&ba, 2); wav.write((char*)&bp, 2);
    wav.write("data", 4); wav.write((char*)&dsz, 4);

    for (int64_t i = 0; i < total_frames * channels; ++i) {
        int32_t v = (int32_t)(samples[i] * 32767.0f);
        int16_t x = (int16_t)std::clamp(v, -32768, 32767);
        wav.write((char*)&x, 2);
    }
}

static bool loudnorm_process_file(const std::string& in_path,
    const std::string& out_path,
    double target_lufs = -23.0) {
    // 加载 DLL
    HMODULE hDll = LoadLibraryA("AudioLoudnorm.dll");
    if (!hDll) {
        char exe[MAX_PATH];
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        PathRemoveFileSpecA(exe);
        std::string dll_path = std::string(exe) + "\\AudioLoudnorm.dll";
        hDll = LoadLibraryA(dll_path.c_str());
    }
    if (!hDll) {
        std::cerr << "Cannot load AudioLoudnorm.dll\n";
        std::cerr << "Make sure all 5 DLLs are in the same directory as the .exe\n";
        return false;
    }

    auto pCreate = (LoudnormCreate)GetProcAddress(hDll, "Loudnorm_Create");
    auto pProcess = (LoudnormProcess)GetProcAddress(hDll, "Loudnorm_Process");
    auto pGetOutput = (LoudnormGetOutput)GetProcAddress(hDll, "Loudnorm_GetOutput");
    auto pFlush = (LoudnormFlush)GetProcAddress(hDll, "Loudnorm_Flush");
    auto pGetStats = (LoudnormGetStats)GetProcAddress(hDll, "Loudnorm_GetStats");
    auto pDestroy = (LoudnormDestroy)GetProcAddress(hDll, "Loudnorm_Destroy");
    auto pVersion = (LoudnormVersion)GetProcAddress(hDll, "Loudnorm_Version");

    if (!pCreate || !pProcess || !pGetOutput || !pFlush || !pDestroy) {
        std::cerr << "Failed to get function pointers\n";
        FreeLibrary(hDll);
        return false;
    }

    std::cout << "AudioLoudnorm version: " << pVersion() << "\n";

    WavData wav = read_wav_float(in_path);
    if (wav.samples.empty()) {
        FreeLibrary(hDll);
        return false;
    }

    std::cout << "Input: " << wav.sample_rate << " Hz, "
        << wav.channels << " ch, "
        << wav.total_frames << " frames\n";

    // 创建 session（6 个参数）
    double target_lra = 7.0;    // EBU R128 默认
    double target_tp = -2.0;   // EBU R128 默认
    int linear_mode = 0;      // 0 = 动态模式

    void* session = pCreate(
        wav.sample_rate,
        wav.channels,
        target_lufs,
        target_lra,
        target_tp,
        linear_mode
    );
    if (!session) {
        std::cerr << "Failed to create loudnorm session\n";
        FreeLibrary(hDll);
        return false;
    }

    // 处理音频（交错数据，一次性传入）
    int ret = pProcess(session, wav.samples.data(), (int)wav.total_frames);
    if (ret < 0) {
        std::cerr << "Loudnorm_Process failed (err=" << ret << ")\n";
        pDestroy(session);
        FreeLibrary(hDll);
        return false;
    }
    std::cout << "Processed " << ret << " frames\n";

    // 获取输出（先 Flush 取剩余，再 GetOutput 取全部）
    std::vector<float> output_all(wav.samples.size());
    int got = pFlush(session, output_all.data(), (int)wav.total_frames);
    if (got <= 0) {
        got = pGetOutput(session, output_all.data(), (int)wav.total_frames);
    }
    if (got <= 0) {
        std::cerr << "Failed to get output samples\n";
        pDestroy(session);
        FreeLibrary(hDll);
        return false;
    }

    int64_t out_frames = got;
    output_all.resize(out_frames * wav.channels);
    std::cout << "Output: " << out_frames << " frames\n";

    // 获取统计信息
    LoudnormStats stats = {};
    if (pGetStats && pGetStats(session, &stats) == 0) {
        std::cout << "Loudnorm stats:\n"
            << "  Integrated: " << stats.integrated_loudness << " LUFS\n"
            << "  Range:      " << stats.loudness_range << " LU\n"
            << "  True Peak:  " << stats.true_peak << " dBTP\n"
            << "  Threshold:  " << stats.threshold << " LUFS\n"
            << "  Offset:     " << stats.offset << " dB\n";
    }

    write_wav_float(out_path, output_all.data(), out_frames,
        wav.sample_rate, wav.channels);

    std::cout << "Output: " << out_path << " (" << out_frames << " frames)\n";

    pDestroy(session);
    FreeLibrary(hDll);
    return true;
}

// ── main ─────────────────────────────────────────────────────
int main()
{
    SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);

    std::cout << "===============================================\n";
    std::cout << "  ADOFAI HitSound Generator\n";
    std::cout << "  Github: https://github.com/Maicy0609\n";
    std::cout << "  Repo:   https://github.com/Maicy0609/ADOFAI_HitSound\n";
    std::cout << "  Bilibili: https://space.bilibili.com/630056484\n";
    std::cout << "===============================================\n\n";

    std::string path;
    std::cout << "adofai path: "; std::getline(std::cin, path);
    if (!path.empty() && path.front() == '"') path.erase(0, 1);
    if (!path.empty() && path.back() == '"') path.pop_back();

    auto tiles = load_adofai(path);
    std::string out = path.substr(0, path.find_last_of('.')) + ".wav";
    generate(tiles, out);

    // ── 询问是否进行响度平衡 ─────────────────────────────
    std::cout << "\n========================================\n";
    std::cout << "Apply loudness normalization? (y/n): ";
    char choice;
    std::cin >> choice;
    std::cin.ignore();

    if (choice == 'y' || choice == 'Y') {
        double target = -23.0;
        std::cout << "Target LUFS (default -23.0 = EBU R128): ";
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty()) target = std::stod(input);

        std::string norm_out = path.substr(0, path.find_last_of('.')) + "_norm.wav";
        std::cout << "Output: " << norm_out << "\n";

        if (loudnorm_process_file(out, norm_out, target)) {
            std::cout << "Loudness normalization complete!\n";
        }
        else {
            std::cout << "Normalization failed. Raw file saved as " << out << "\n";
        }
    }

    std::cout << "press enter..."; std::cin.get();
}