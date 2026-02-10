# ADOFAI HitSound

将 `.adofai` 谱面文件转换为打击音效的高性能工具。

## 下载

- [HitSound.exe](https://github.com/Maicy0609/ADOFAI_HitSound/raw/main/x64/Release/HitSound.exe)
- [hit.wav](https://github.com/Maicy0609/ADOFAI_HitSound/blob/main/x64/Release/hit.wav)

> 请将两个文件置于同一目录下运行。

## 使用方法

1. 确保 `HitSound.exe` 与 `hit.wav` 位于同一文件夹
2. 启动 `HitSound.exe` 在交互中，将 `.adofai` 谱面文件拖入
3. 输出文件将生成于谱面同目录

## 特性

- 高性能处理（基于 RapidJSON）
- 支持自定义音高参数（`pitch`）
- 附带 Python 参考实现（`HitSound.py`）

## 性能实测

测试环境：Intel Core i5-9600T @ 2.30GHz  
谱面大小：1.46 GB (1,575,693,095 字节)

| pitch | 耗时  |
|-------|-------|
| 25    | ~13 s |
| 100   | ~27 s |

## 依赖

- [RapidJSON](https://github.com/Tencent/rapidjson)

## 注意事项

- 请确保系统已安装 Visual C++ 运行库
- 如遇运行问题，请检查文件完整性及依赖环境
```
