# Architecture

## 数据流

```text
MainWindow
  ├─ ProfileStore ── DPAPI / QSettings
  ├─ ModelService ── ProtocolAdapter ── QNetworkAccessManager
  └─ BenchmarkController
       ├─ bounded queue (1..8 concurrent)
       ├─ BenchmarkJob per request
       │    ├─ ProtocolAdapter request plan
       │    ├─ QNetworkReply async stream
       │    ├─ SseDecoder
       │    └─ normalized TestResult
       └─ ResultRepository ── SQLite WAL
```

## 关键边界

- UI 不直接解析协议；只消费统一的 `TestResult`。
- SSE 帧拆分与协议 JSON 解析分离。
- 每个 `BenchmarkJob` 独立持有网络管理器、解析状态和高精度计时器。
- OpenAI 的回退计划是数据结构而不是 UI 分支。
- 模型详情探测限制 8 并发和 12 秒预算，并带取消 generation，避免旧请求污染新结果。
- 历史记录使用 SQLite WAL；Profile 密钥使用 Windows DPAPI。

## 计时定义

- TTFT：整个测试从首次发出请求到首个非空文本 delta 的时间，自动回退开销会被计入。
- Generation：首个文本 delta 到该 attempt 完成的时间。
- Total latency：整个 BenchmarkJob 从开始到最终完成，包含必要回退。
- Response bytes：全部 attempt 实际接收字节累计。

## 容错策略

- 混合 CR/LF/CRLF 和任意 chunk 边界。
- SSE JSON 畸形事件记录 warning 后继续；明确 API error 则终止。
- HTML/WAF 页面单独识别。
- 成功 HTTP 但返回普通 JSON 时尝试非流式解析并记录提示。
- 原始响应按 attempt 分段，便于定位回退链。
