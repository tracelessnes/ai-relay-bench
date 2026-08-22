# AI Relay Station Benchmark & Tester

一个面向 AI 中转站、自建反代与官方兼容端点的现代化 Qt 6 桌面测速工具。用于验证 OpenAI Chat Completions、Anthropic Claude Messages 与 OpenAI Responses/Codex 三类协议的可用性、流式兼容性、延迟、吞吐和多轮稳定性。

## 功能

### 三协议兼容引擎

- **OpenAI**：自动尝试 `/chat/completions`、`/v1/chat/completions`；按“流式 + usage → 纯流式 → 非流式”降级。
- **Claude**：兼容 `/messages`、`/v1/messages`；解析 `message_start`、`content_block_delta`、`message_delta`、`message_stop`；支持 Claude 1M beta 请求头。
- **Codex / Responses**：调用 `/v1/responses`；自动生成官方客户端兼容头和每请求 UUID；解析 Responses SSE 并避免 `output_text.delta` 与 `output_item.done` 重复拼接。

### 测速与稳定性

- 毫秒级 TTFT、First Byte、生成耗时、总延迟
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

## 工程结构

```text
src/
├── domain/             # 领域类型、序列化、Token 估算、统计
├── protocol/           # ProtocolAdapter、三协议实现、SSE 解码器
├── network/            # 单请求 BenchmarkJob 与精确计时/回退
├── services/           # 测试调度、模型发现、Profile、导出
├── persistence/        # DPAPI 安全存储、SQLite ResultRepository
└── ui/                 # MainWindow、暗色界面、趋势图
resources/              # QSS 与 Qt 资源
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

## Windows 发布包

仓库中的 `scripts/package-windows.ps1` 会执行 Release 构建、测试、`windeployqt`、复制许可证和文档，并生成 ZIP：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

发布目录：`dist/AI-Relay-Bench-1.1.0-win64/`
ZIP：`dist/AI-Relay-Bench-1.1.0-win64.zip`

## 数据与安全

- Profile 存储在当前 Windows 用户的应用设置中，并由 DPAPI 加密。
- 测试历史保存在 `QStandardPaths::AppLocalDataLocation` 下的 `benchmark-history.sqlite`。
- 原始响应最多保存 4 MiB；SSE 单次未处理缓冲限制为 8 MiB。
- Profile 导出会清除 API Key 和代理密码。

## 测试覆盖

- 上下文长度解析、输出清洗、Token 估算、百分位统计
- CRLF / CR / LF、跨网络 chunk、超大 SSE 缓冲
- OpenAI `stream_options` 400 回退并继续流式测试
- Claude usage 归一化
- Codex delta / done 去重
- 畸形 SSE JSON 软容错

> 注意：发布包和自动测试不包含任何真实 API Key。不同中转站仍可能存在私有鉴权、WAF 或非标准协议差异，可通过原始响应查看器和自定义请求头进一步诊断。
