# mouse_light (MouseRipple)

> 🎨 **现代化 Windows 鼠标水波纹与水墨流线拖尾特效工具**  
> 专为教学演示、屏幕录制、直播与高效办公设计。纯原生 C++17 构建，零第三方依赖，极致轻量且优雅通透。

---

## ✨ 核心特性

- 🌊 **双层空灵水波纹（渐进羽化）**：
  - 采用 Asymptotic Gaussian Falloff 算法，边缘柔和消融于背景，彻底消除生硬的贴纸切边；
  - 保持中心通透不遮挡光标及点击内容；底层微炭灰折射阴影，在浅色、纯白、深色背景下均清晰醒目。
- 🖌️ **水墨流线拖尾（Catmull-Rom 连续微线流）**：
  - 高密度样条微步插值（1.5px/步）与全圆头无缝接合，彻底告别折痕、肋条拼接缝与急转弯死褶；
  - 具备淡雅水墨晕光与笔尖饱满墨滴，头部随光标饱满圆润、尾部渐细羽化消散如烟；
  - 拖尾开关、颜色自选调色板、留存时长（150~800ms）、粗细（3~18px）、透明度（10%~100%）自由微调。
- ⭕ **常驻单圈纯净呼吸光圈**：
  - 单圈舒缓呼吸循环，纯净线条跟随鼠标，支持 1~6px 粗细与 0%~100% 透明度调节。
- 🎛️ **可视化高分屏自适应设置面板**：
  - 支持 Wacom 笔触风格、经典水波涟漪、自定义自由参数等预设模式；
  - 鼠标左键、右键、中键、常驻光圈、移动拖尾全部支持独立调色（呼出 Windows 标准调色盘）；
  - 双击系统托盘图标或再次运行程序均可快速唤出设置面板。
- ⚡ **极致轻量与零功耗休眠**：
  - 纯 C++17 + 原生 Win32 API + GDI+ 硬件加速级渲染，内存占用仅 ~15MB；
  - `WS_EX_TRANSPARENT` 全屏穿透，绝不拦截任何鼠标事件；
  - 当鼠标静止且水波/拖尾消散后，渲染循环自动挂起，**待机 CPU 占用严格保持 0.0%**。
- 💾 **自动持久化与开机自启动**：
  - 所有配置即时保存至同目录 `MouseRipple.ini`；支持系统托盘右键一键开启/关闭开机自启动。

---

## 📥 下载与运行

前往 [Releases 页面](https://github.com/secure-artifacts/mouse_light/releases) 下载最新发布的构建产物：

- **`mouse_light.exe`**：单文件绿色免安装版，双击即可直接运行；
- **`mouse_light-windows-x64.zip`**：包含完整预设配置与说明文件的压缩包。

---

## 🔐 验证软件来源（安全合规）

本项目所有正式 Release 均通过 GitHub Actions 官方 CI 构建，并生成了经过加密签名的 **Artifact Attestation (SLSA Provenance v1.0)**，完全符合平台 L2 安全标准。

下载产物后，可在终端使用 GitHub CLI 验证文件完整性与官方来源：

```bash
gh attestation verify ./mouse_light.exe --repo secure-artifacts/mouse_light
```

若验证成功，将输出签名的 Provenance 凭证，确认该文件确实由官方 CI 直接构建且未被任何第三方篡改。

---

## 🛠️ 本地编译构建

本项目无需安装任何第三方库，仅需安装支持 C++17 的 Visual Studio（2019 或 2022）：

### 方式一：一键脚本构建
直接双击运行仓库根目录的 `build.bat`，脚本将自动检测环境并编译出 `MouseRipple.exe`。

### 方式二：命令行手动编译
在 **Developer Command Prompt for VS 2022** 中运行：

```cmd
cl /nologo /EHsc /O2 /std:c++17 /DUNICODE /D_UNICODE MouseRipple.cpp /link /SUBSYSTEM:WINDOWS gdiplus.lib user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib advapi32.lib /OUT:MouseRipple.exe
```

### 方式三：CMake 构建
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

---

## 📄 开源许可

本项目基于 [MIT License](LICENSE) 协议开源。
