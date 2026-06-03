# 🦆 ESP-DUCK

ESP32-S3 USB HID keyboard sender with Web Console.

ESP-DUCK 是一个基于 ESP32 / ESP32-S3 的 USB HID 控制项目，可通过 Web Console 控制开发板向目标电脑发送键盘输入、命令文本或脚本内容。

适用于个人开发、内网调试、自动化输入、受限环境下的文本传送测试。

## ✨ Features

* ⌨️ USB HID 虚拟键盘输入
* 🌐 Web Console 控制发送内容
* 📦 设备内置 WebUI，适合无外部网络环境使用
* 🧾 支持普通文本、命令、脚本片段发送
* ⚙️ 支持变量配置，例如延迟、设备名、Token 等
* 🀄 支持中文内容预处理，例如拼音转写后发送
* 💾 支持 LittleFS / SPIFFS 存放前端页面
* 🔐 密码和 Token 使用占位符，避免真实敏感信息写入仓库

## 📁 Project Structure

    .
    ├── README.md
    ├── LICENSE
    ├── .gitignore
    ├── src/
    │   └── main.ino
    ├── data/
    │   └── index.html
    └── online.html

## 🔐 Default Placeholders

请在本地修改 `src/main.ino` 顶部配置：

    const char* WEB_PASSWORD = "CHANGE_ME_WEB_PASSWORD";
    const char* SETUP_AP_SSID = "ESP-DUCK";
    const char* SETUP_AP_PASS = "CHANGE_ME_12345678";
    const char* DEVICE_NAME = "ESP-DUCK";

注意：`SETUP_AP_PASS` 至少 8 位。

## 🚀 Usage

### 1. Prepare

可选择以下开发环境：

* Arduino IDE
* PlatformIO
* ESP-IDF

请确认已经安装 ESP32 / ESP32-S3 开发板支持。

### 2. Build & Upload

PlatformIO 示例：

    pio run
    pio run --target upload

上传 WebUI 文件：

    pio run --target uploadfs

Arduino IDE 用户可使用 LittleFS / SPIFFS 上传插件。

### 3. Open Web Console

开发板启动后，通过串口日志查看访问地址。

打开 Web Console 后，即可发送文本、命令或脚本片段。

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

## 🧭 Roadmap

* [ ] 优化 Web 串口连接稳定性
* [ ] 增加日志输出限流
* [ ] 增加断开连接后的资源释放
* [ ] 支持上电恢复默认配置
* [ ] 支持物理按键快捷操作
* [ ] 支持文本发送队列
* [ ] 支持中文内容拼音转写
* [ ] 支持配置导入和导出

## 📄 License

MIT License.

## ⚠️ Disclaimer

本项目仅用于个人学习、自动化输入测试、内网调试和合法授权环境。

请勿用于未授权设备、未授权系统或任何违反法律法规的场景。
