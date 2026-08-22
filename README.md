# AI Relay Station Benchmark & Tester

一个面向 AI 中转站、自建反代与官方兼容端点的现代化 Qt 6 桌面测速工具。用于验证 OpenAI Chat Completions、Anthropic Claude Messages 与 OpenAI Responses/Codex 三类协议的可用性、流式兼容性、延迟、吞吐和多轮稳定性。

当前版本：**V1.2.0**

## 功能

### 三协议兼容引擎

- **OpenAI**：自动尝试 `/chat/completions`、`/v1/chat/completions`；按“流式 + usage → 纯流式 → 非流式”降级。
- **Claude**：兼容 `/messages`、`/v1/messages`；解析 `message_start`、`content_block_delta`、`message_delta`、`message_stop`；支持 Claude 1M beta 请求头。
- **Codex / Responses**：调用 `/v1/responses`；自动生成官方客户端兼容头和每请求 UUID；解析 Responses SSE 并避免 `output_text.delta` 与 `output_item.done` 重复拼接。

### 测速与稳定性

- 毫秒级 TTFT、First Byte、First Text、生成耗时、总延迟，以及 DNS/TCP/TLS/请求阶段计时
- Tokens/s、响应体积、Prompt / Completion / Total usage
- usage 缺失时按中文字符与英文约 4 字符估算
- 精确 `OK` 判定，或自定义整串正则判定
- 单次、全模型批量、多轮稳定性 / Jitter 测试
- Min / Avg / Max / P50 / P95、通过率、丢包率、TTFT 抖动率
- 最近 60 次 TTFT / 总延迟 / Tokens/s 趋势图

### 站点、模型和网络

- 多 Profile CRUD、Windows DPAPI 本地加密保存
- Profile JSON 导入导出（出于安全考虑不会导出 API Key 和代理密码）
- 自定义请求头 JSON 实时校验及 Codex CLI、Claude Code、Claude 1M、Cherry Studio、ChatBox 预设
- 模型列表拉取；最多 8 并发、12 秒预算查询详情
- 同时尝试 `/models/{id}` 与 `/v1/models/{id}` 详情路径
- `models.dev/api.json` 上下文窗口兜底
- 系统代理、直连、HTTP、SOCKS5，支持代理用户名和密码

### 结果和诊断

- 实时指标卡和流式输出
- HTTP 状态、结构化错误字段、HTML 拦截页识别
- 原始 JSON / SSE 查看器
- SQLite 测试历史持久化，启动时加载最近 500 条
- 结果筛选和会话清除
- JSON / CSV / Markdown 报表导出

## V1.2 新增功能

### 协议扫描

点击主界面的“站点扫描”后，程序会针对当前 Profile 和模型依次检查模型列表与当前协议的流式端点，并同时处理带 `/v1` 和不带 `/v1` 的候选路径。扫描结果逐项显示 HTTP 状态和诊断信息，并按通过项计算 0–100 分；成功响应、404/405、其他 4xx/5xx、网络错误、超时及 HTML/WAF 拦截页会分别归类，便于快速定位兼容性问题。

### 多站点横向对比

点击“横向对比”可将当前会话的测试结果按 Profile 名称分组，比较测试次数、通过率、平均 TTFT、P50/P95 TTFT、平均总延迟、P50/P95 总延迟和平均 Tokens/s。对比表只使用结果中的站点、模型和归一化统计，不需要重新发起请求。

### 语言包与主题

- 内置 `zh-CN` 与 `en-US` 语言包，可在“系统设置”中手动切换或跟随系统语言。
- 支持导入 JSON 语言包；用户语言包保存在应用数据目录，大小限制为 2 MiB，切换后界面立即刷新。
- 标题栏支持深色、浅色主题切换；也支持跟随系统配色，设置保存在当前用户的应用设置中。

### 脱敏与安全

测试结果、扫描诊断和报表导出会对 JSON、SSE、HTTP 头及普通文本中的 `authorization`、API Key、Cookie、密码、secret、token 等敏感字段进行脱敏。短值会完整替换，较长值仅保留少量首尾字符用于定位；发布包、测试夹具和 CI 检查不包含真实 API Key、代理密码或其他真实密钥。

## 使用说明

1. 启动程序后新建或选择一个 Profile，填写站点地址、协议、API Key、代理和请求头；站点地址可省略 `/v1`，程序会自动尝试兼容路径。
2. 点击“获取模型”拉取模型列表，或直接输入模型 ID；可在测试配置中设置 Prompt、最大 Token、超时、并发数和通过判定规则。
3. 先点击“站点扫描”检查模型列表与协议端点，再选择“单次测试”“测试全部模型”或“稳定性 / 抖动”运行基准测试。
4. 双击结果行查看已脱敏的原始 JSON / SSE；使用筛选框按模型、协议、状态或错误过滤结果。
5. 点击“横向对比”查看当前会话的多站点统计；点击“导出报表”导出 JSON、CSV 或 Markdown。
6. 在系统设置中切换语言、导入语言包；使用标题栏主题按钮切换深色/浅色主题。

## 工程结构

```text
src/
├── domain/             # 领域类型、序列化、Token 估算、统计与横向对比
├── protocol/           # ProtocolAdapter、三协议实现、SSE 解码器
├── network/            # 单请求 BenchmarkJob 与精确计时/回退
├── services/           # 测试调度、协议扫描、模型发现、Profile、导出
├── security/           # 原始响应、诊断文本和报表的脱敏
├── persistence/        # DPAPI 安全存储、SQLite ResultRepository
├── i18n/               # 内置/用户语言包与运行时翻译
└── ui/                 # MainWindow、扫描/对比窗口、主题与趋势图
resources/              # QSS、语言包与 Qt 资源
tests/                  # 核心单测和本地 Mock HTTP/SSE 集成测试
```

## 构建

要求：Qt 6.7+（Core、Gui、Widgets、Network、Sql、Test）、CMake 3.24+、支持 C++20 的编译器。

```powershell
cmake -S . -B build-qt -G Ninja `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\mingw_64 `
  -DCMAKE_CXX_COMPILER=C:\Qt\Tools\mingw1310_64\bin\g++.exe `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-qt -j 4
```

运行测试：

```powershell
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
ctest --test-dir build-qt --output-on-failure
```

运行开发构建：

```powershell
.\build-qt\ai-relay-bench.exe
```

## CI

`.github/workflows/windows.yml` 在 Windows runner 上执行以下检查：

- 安装 Qt 6.7.3 MinGW，配置并构建 CMake Release 工程
- 运行 CTest 核心测试和集成测试
- 执行 `scripts/check-sensitive.ps1`，扫描版本库产品文件中的疑似真实密钥

工作流不会写入或使用真实 API Key；本地运行 CI 检查时也应只使用合成测试数据。

## Windows 发布包

仓库中的 `scripts/package-windows.ps1` 会读取 `CMakeLists.txt` 中的 `project(... VERSION ...)`，执行 Release 构建、测试、`windeployqt`、复制 README，并生成 ZIP：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

当前版本的发布目录：`dist/AI-Relay-Bench-1.2.0-win64/`
当前版本的 ZIP：`dist/AI-Relay-Bench-1.2.0-win64.zip`

## 数据与安全

- Profile 存储在当前 Windows 用户的应用设置中，并由 DPAPI 加密。
- 测试历史保存在 `QStandardPaths::AppLocalDataLocation` 下的 `benchmark-history.sqlite`。
- 原始响应最多保存 4 MiB；SSE 单次未处理缓冲限制为 8 MiB。
- 运行时结果和导出报表默认经过脱敏；Profile 导出会清除 API Key 和代理密码。

## 测试覆盖

- 上下文长度解析、输出清洗、Token 估算、百分位统计与多站点分组对比
- CRLF / CR / LF、跨网络 chunk、超大 SSE 缓冲
- OpenAI `stream_options` 400 回退并继续流式测试
- Claude usage 归一化
- Codex delta / done 去重
- 畸形 SSE JSON 软容错
- 协议扫描的端点候选、评分、序列化和本地 Mock 流程
- 脱敏服务对 JSON、SSE、请求头、嵌套字段和导出路径的覆盖

> 注意：发布包和自动测试不包含任何真实 API Key。不同中转站仍可能存在私有鉴权、WAF 或非标准协议差异，可通过扫描结果、自定义请求头和已脱敏的原始响应查看器进一步诊断。
