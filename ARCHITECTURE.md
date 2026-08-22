# Architecture / 架构

> 当前文档对应 V1.2.0。

## 数据流

```text
MainWindow
  ├─ ProfileStore ── DPAPI / QSettings
  ├─ ModelService ── ProtocolAdapter ── QNetworkAccessManager
  ├─ ScanDialog ── ScanService ── ProtocolAdapter ── QNetworkAccessManager
  └─ BenchmarkController
       ├─ bounded queue (1..8 concurrent)
       ├─ BenchmarkJob per request
       │    ├─ ProtocolAdapter request plan
       │    ├─ QNetworkReply async stream
       │    ├─ SseDecoder
       │    └─ RedactionService ── normalized TestResult
       ├─ Statistics::compareResults ── CompareDialog
       └─ ResultRepository ── SQLite WAL

SettingsDialog ── LanguageManager ── builtin/user JSON language packs
TitleBar / TrendChart ── ThemeManager ── light.qss / dark.qss
```

## 关键边界

- UI 不直接解析协议；`MainWindow`、`ScanDialog` 和 `CompareDialog` 只消费服务层或领域层的统一结果。
- `ProtocolAdapter` 负责三种协议的请求计划、认证头、流式/非流式解析；`SseDecoder` 只负责帧边界和事件拆分。
- `BenchmarkJob` 独立持有网络回复、解析状态和高精度计时器；单个请求的自动回退不会污染其他 Job。
- `ScanService` 顺序执行模型列表和当前协议流式检查，统一生成带 `/v1` 与不带 `/v1` 的端点候选；扫描只报告分类结果和脱敏诊断，不改变基准测试状态。
- `Statistics::compareResults()` 按脱敏后的 Profile 名称聚合当前会话结果，计算次数、通过率、平均值、P50/P95 与平均速度；对比窗口不重新发起网络请求。
- `RedactionService` 位于原始响应、错误摘要、扫描诊断和报表边界：递归处理 JSON，也处理 SSE `data:` 行、HTTP 头和普通文本；敏感字段不会进入导出内容。
- `LanguageManager` 负责内置/用户 JSON 语言包、系统语言选择和运行时重翻译；`ThemeManager` 负责系统、深色和浅色主题及持久化。
- 模型详情探测限制 8 并发和 12 秒预算，并带取消 generation，避免旧请求污染新结果。
- 历史记录使用 SQLite WAL；Profile 密钥使用 Windows DPAPI。

## V1.2 组件职责

| 组件 | 职责 | 输入 / 输出 |
|---|---|---|
| `ScanService` | 扫描模型列表和协议流式端点，分类 HTTP、网络、超时与 HTML/WAF 结果 | `Profile`、模型 ID → `ScanResult` / `ScanCheck` |
| `Statistics` | 计算百分位、均值、抖动和多站点汇总 | `TestResult` 列表 → `CompareSummary` 列表 |
| `RedactionService` | 对敏感键值做结构化和文本脱敏 | JSON、SSE、HTTP 头、普通文本 → 脱敏文本/值 |
| `LanguageManager` | 加载内置/用户语言包并刷新界面 | JSON 语言包 → 当前翻译 |
| `ThemeManager` | 持久化并应用系统/深色/浅色 QSS | 主题设置 → Qt 应用样式 |

## 计时定义

- TTFT：整个测试从首次发出请求到首个非空文本 delta 的时间，自动回退开销会被计入。
- First Byte：当前 attempt 收到第一个网络字节的时间点。
- First Text：首次收到可显示文本 delta 的时间点。
- Generation：首个文本 delta 到该 attempt 完成的时间。
- Total latency：整个 `BenchmarkJob` 从开始到最终完成，包含必要回退。
- Response bytes：全部 attempt 实际接收字节累计。

## 容错策略

- 混合 CR/LF/CRLF 和任意 chunk 边界。
- SSE JSON 畸形事件记录 warning 后继续；明确 API error 则终止。
- HTML/WAF 页面单独识别；404/405 作为兼容性扫描中的 warning，其他非 2xx 通常判为失败。
- 成功 HTTP 但返回普通 JSON 时尝试非流式解析并记录提示。
- 原始响应按 attempt 分段，便于定位回退链；进入结果、扫描诊断和导出路径前统一脱敏。
- 单个网络请求使用超时和取消路径，服务完成后通过 Qt 信号回到 UI，不在 UI 线程执行阻塞请求。

## CI 与发布

GitHub Actions 的 Windows 工作流使用 Qt 6.7.3 MinGW，执行 CMake Release 构建、CTest 和敏感信息扫描。发布脚本从顶层 `CMakeLists.txt` 的 `project(... VERSION ...)` 读取版本，不单独维护发布脚本版本常量，确保产物目录、ZIP 名称和编译期 `APP_VERSION` 一致。
