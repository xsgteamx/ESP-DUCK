# 🦆 ESP-DUCK

ESP32-S3 USB HID keyboard sender with Web Console.

ESP-DUCK 是一个基于 ESP32 / ESP32-S3 的 USB HID 控制项目，可通过 Web Console 控制开发板向目标电脑发送键盘输入、命令文本或脚本内容。

适用于个人开发、内网调试、自动化输入和受限环境下的文本传送测试。

## ✨ Features

* ⌨️ USB HID 虚拟键盘输入
* 🌐 Web Console 控制发送内容
* 📦 设备内置 WebUI，适合无外部网络环境使用
* 🔌 支持 USB Serial / Wi-Fi 两种控制方式
* 🧾 支持普通文本、命令、脚本片段发送
* ⚙️ 支持发送延迟、分块、行延迟、倒计时等参数
* 🧊 支持保活动作，例如鼠标移动、点击、F5、点击 + F5
* 📶 支持 Wi-Fi 扫描、保存、清空配置
* 🀄 支持中文内容预处理，例如跳过中文或拼音转写后发送
* 💾 支持 LittleFS / SPIFFS 存放设备内置页面
* 🔐 使用 `secrets.h` 管理本地密码和 Token，避免敏感信息进入仓库

## 📁 Project Structure

    .
    ├── README.md
    ├── LICENSE
    ├── .gitignore
    ├── index.html
    ├── src/
    │   ├── main.ino
    │   └── secrets.example.h
    └── data/
        └── index.html

本地开发时，需要额外创建但不要提交：

    src/secrets.h

## 🔐 Local Secrets

仓库只提交 `src/secrets.example.h`，真实密码写在本地的 `src/secrets.h`。

复制示例文件：

    cp src/secrets.example.h src/secrets.h

然后修改 `src/secrets.h`：

    const char* WEB_PASSWORD = "CHANGE_ME_WEB_PASSWORD";
    const char* SETUP_AP_SSID = "SG_Duck";
    const char* SETUP_AP_PASS = "CHANGE_ME_12345678";
    const char* DEVICE_NAME = "SG_Duck";

说明：

* `WEB_PASSWORD`：设备 Web 登录密码，同时也是 `/api/message` Token
* `SETUP_AP_SSID`：开发板配置热点名称
* `SETUP_AP_PASS`：开发板配置热点密码，至少 8 位
* `DEVICE_NAME`：设备显示名称

`src/secrets.h` 已加入 `.gitignore`，不要提交到公开仓库。

如果真实密码或 Token 曾经提交到公开仓库，请直接更换。

## 🚀 Usage

### 1. Prepare

推荐使用：

* Arduino IDE
* PlatformIO，使用 Arduino framework

请确认已经安装：

* ESP32 / ESP32-S3 开发板支持
* ArduinoJson
* LittleFS / SPIFFS 上传工具

说明：这是 `.ino` 项目，不是原生 ESP-IDF 工程。直接用 ESP-IDF 编译并不合适，除非你自行改成 ESP-IDF + Arduino component 结构。

### 2. Configure Secrets

首次使用前先创建本地密钥文件：

    cp src/secrets.example.h src/secrets.h

然后打开 `src/secrets.h` 修改密码、热点名称和设备名。

### 3. Build & Upload

PlatformIO 示例：

    pio run
    pio run --target upload

上传 WebUI 文件：

    pio run --target uploadfs

Arduino IDE 用户可打开 `src/main.ino` 编译上传。上传 `data/` 目录时，请使用 LittleFS / SPIFFS 上传插件。

### 4. Open Web Console

开发板启动后，通过串口日志查看访问地址。

打开设备 WebUI 后，即可发送文本、命令或脚本片段。

### 5. Online Console

根目录 `index.html` 是外部控制台页面，可部署到 GitHub Pages / Vercel。

注意：公网 HTTPS 页面访问局域网 HTTP 设备时，可能受到浏览器 mixed content、CORS 或 Private Network Access 限制。遇到拦截时，建议直接使用设备内置 WebUI。

## 📝 Notes

### 🧊 Browser freezes after connection

如果刚连接开发板后浏览器明显卡顿，优先检查：

* Web 页面是否重复注册 reader / listener / timer
* 串口读取循环是否没有正确释放
* 日志是否持续追加到 DOM
* 设备端是否在高速输出日志
* 页面是否缺少日志数量限制或输出限流

建议先用最简连接页面测试，只保留连接、断开和少量日志输出。

### 🀄 Chinese input

USB HID 键盘输入不适合直接发送中文。

推荐方案：

* 中文转拼音
* 中文转英文
* Base64 后再传输
* 在目标环境内再还原内容

如果设备内置页要使用“中文转拼音”，需要将 `pinyin-pro.js` 放入 LittleFS / SPIFFS 根目录，或改为在线 CDN 版本。

## 🧭 Roadmap

* [x] USB HID 虚拟键盘输入
* [x] 设备内置 WebUI
* [x] USB Serial 控制
* [x] Wi-Fi 局域网控制
* [x] 文本暂存
* [x] 发送速度档位
* [x] Base64 纯输出模式
* [x] Wi-Fi 扫描与配置
* [x] 保活模式
* [x] 本地 secrets 配置
* [ ] 优化 Web 串口连接稳定性
* [ ] 增加日志输出限流
* [ ] 增加断开连接后的资源释放
* [ ] 支持上电恢复默认配置
* [ ] 支持物理按键快捷操作
* [ ] 支持配置导入和导出

## 📄 License

MIT License.

## ⚠️ Disclaimer

本项目仅用于个人学习、自动化输入测试、内网调试和合法授权环境。

请勿用于未授权设备、未授权系统或任何违反法律法规的场景。
