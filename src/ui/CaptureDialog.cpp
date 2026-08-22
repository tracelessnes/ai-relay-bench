#include "CaptureDialog.h"
#include "capture/CaptureProxyServer.h"
#include "capture/HarExporter.h"
#include "i18n/LanguageManager.h"
#include "ui/TitleBar.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace airb {
namespace {
QString tr(const char* key, const char* fallback) {
    return LanguageManager::instance()->trKey(QString::fromLatin1(key),
                                               QString::fromLatin1(fallback));
}

QString displayBody(const QByteArray& body) {
    if (body.isEmpty()) return {};
    const QString text = QString::fromUtf8(body);
    if (!text.isEmpty()) return text;
    return QString::fromLatin1(body.toHex(' '));
}

QString captureClientLabel(CaptureClient client) {
    return captureClientName(client);
}
}

CaptureDialog::CaptureDialog(QWidget* parent) : QDialog(parent), proxy_(new CaptureProxyServer(this)) {
    setObjectName(QStringLiteral("captureDialog"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    resize(1180, 760);
    setMinimumSize(900, 600);

    auto* shell = new QVBoxLayout(this);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    titleBar_ = new DialogTitleBar(this, this);
    shell->addWidget(titleBar_);

    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("captureBody"));
    auto* root = new QVBoxLayout(body);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);
    shell->addWidget(body, 1);

    auto* heading = new QHBoxLayout;
    title_ = new QLabel;
    title_->setObjectName(QStringLiteral("contentTitle"));
    state_ = new QLabel;
    state_->setObjectName(QStringLiteral("secondaryText"));
    heading->addWidget(title_);
    heading->addStretch();
    heading->addWidget(state_);
    root->addLayout(heading);

    auto* controls = new QHBoxLayout;
    controls->setSpacing(8);
    auto* addressLabel = new QLabel;
    addressLabel->setObjectName(QStringLiteral("captureAddressLabel"));
    address_ = new QLineEdit(QStringLiteral("127.0.0.1"));
    address_->setObjectName(QStringLiteral("captureAddress"));
    address_->setMinimumWidth(150);
    auto* portLabel = new QLabel;
    portLabel->setObjectName(QStringLiteral("capturePortLabel"));
    port_ = new QSpinBox;
    port_->setRange(1, 65535);
    port_->setValue(8765);
    port_->setObjectName(QStringLiteral("capturePort"));
    port_->setMaximumWidth(100);
    start_ = new QPushButton;
    start_->setObjectName(QStringLiteral("primaryButton"));
    stop_ = new QPushButton;
    clear_ = new QPushButton;
    export_ = new QPushButton;
    controls->addWidget(addressLabel);
    controls->addWidget(address_);
    controls->addWidget(portLabel);
    controls->addWidget(port_);
    controls->addSpacing(8);
    controls->addWidget(start_);
    controls->addWidget(stop_);
    controls->addWidget(clear_);
    controls->addWidget(export_);
    controls->addStretch();
    root->addLayout(controls);

    environment_ = new QLabel;
    environment_->setObjectName(QStringLiteral("captureHint"));
    environment_->setWordWrap(true);
    root->addWidget(environment_);

    count_ = new QLabel;
    count_->setObjectName(QStringLiteral("secondaryText"));
    root->addWidget(count_);

    table_ = new QTableWidget(0, 8, body);
    table_->setObjectName(QStringLiteral("captureTable"));
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    root->addWidget(table_, 3);

    detailTitle_ = new QLabel;
    detailTitle_->setObjectName(QStringLiteral("captureDetailTitle"));
    detailTitle_->setWordWrap(true);
    root->addWidget(detailTitle_);

    detailTabs_ = new QTabWidget(body);
    detailTabs_->setObjectName(QStringLiteral("captureDetails"));
    requestHeaders_ = new QPlainTextEdit;
    requestBody_ = new QPlainTextEdit;
    responseHeaders_ = new QPlainTextEdit;
    responseBody_ = new QPlainTextEdit;
    sseEvents_ = new QPlainTextEdit;
    for (auto* editor : {requestHeaders_, requestBody_, responseHeaders_, responseBody_, sseEvents_}) {
        editor->setReadOnly(true);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    }
    detailTabs_->addTab(requestHeaders_, QString());
    detailTabs_->addTab(requestBody_, QString());
    detailTabs_->addTab(responseHeaders_, QString());
    detailTabs_->addTab(responseBody_, QString());
    detailTabs_->addTab(sseEvents_, QString());
    root->addWidget(detailTabs_, 2);

    connect(start_, &QPushButton::clicked, this, &CaptureDialog::startCapture);
    connect(stop_, &QPushButton::clicked, this, &CaptureDialog::stopCapture);
    connect(clear_, &QPushButton::clicked, this, &CaptureDialog::clearCaptures);
    connect(export_, &QPushButton::clicked, this, &CaptureDialog::exportCapture);
    connect(table_, &QTableWidget::cellClicked, this, &CaptureDialog::selectExchange);
    connect(proxy_, &CaptureProxyServer::exchangeReady, this, &CaptureDialog::onExchange);
    connect(proxy_, &CaptureProxyServer::stateChanged, this,
            [this](bool running, const QString& message) { updateState(running, message); });
    connect(LanguageManager::instance(), &LanguageManager::languageChanged,
            this, &CaptureDialog::retranslate);

    updateState(false);
    retranslate();
}

void CaptureDialog::startCapture() {
    QHostAddress address;
    if (!address.setAddress(address_->text().trimmed())) {
        QMessageBox::warning(this, tr("capture.invalidAddress", "Invalid listen address"),
                             tr("capture.invalidAddress", "Please enter a valid listen address."));
        return;
    }
    if (!proxy_->start(address, static_cast<quint16>(port_->value()))) {
        QMessageBox::critical(this, tr("capture.start", "Start capture"), proxy_->errorString());
        return;
    }
    updateState(true);
}

void CaptureDialog::stopCapture() {
    proxy_->stop();
    updateState(false);
}

void CaptureDialog::onExchange(const CaptureExchange& exchange) {
    exchanges_.append(exchange);
    const int row = table_->rowCount();
    table_->insertRow(row);
    const QString time = exchange.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString status = exchange.response.statusCode > 0
        ? QString::number(exchange.response.statusCode)
        : QStringLiteral("-");
    const QString duration = exchange.totalMs >= 0
        ? QString::number(exchange.totalMs, 'f', 1)
        : QStringLiteral("-");
    const QList<QString> values = {
        time,
        exchange.request.method,
        exchange.request.url.isValid() ? exchange.request.url.toString() : exchange.request.target,
        captureClientLabel(exchange.request.client),
        status,
        duration,
        QString::number(exchange.requestBytes),
        QString::number(exchange.responseBytes)
    };
    for (int column = 0; column < values.size(); ++column)
        table_->setItem(row, column, new QTableWidgetItem(values.at(column)));
    table_->selectRow(row);
    showExchange(row);
    count_->setText(tr("capture.count", "Captured %1 request(s)").arg(exchanges_.size()));
}

void CaptureDialog::selectExchange(int row, int column) {
    Q_UNUSED(column);
    showExchange(row);
}

void CaptureDialog::showExchange(int row) {
    if (row < 0 || row >= exchanges_.size()) return;
    const auto& exchange = exchanges_.at(row);
    setHeaders(requestHeaders_, exchange.request.headers);
    requestBody_->setPlainText(displayBody(exchange.request.body));
    setHeaders(responseHeaders_, exchange.response.headers);
    responseBody_->setPlainText(displayBody(exchange.response.body));
    sseEvents_->setPlainText(sseText(exchange.request.sseEvents + exchange.response.sseEvents));
    detailTitle_->setText(exchange.request.method + QStringLiteral(" ") + exchange.request.target);
}

void CaptureDialog::clearCaptures() {
    exchanges_.clear();
    table_->setRowCount(0);
    requestHeaders_->clear();
    requestBody_->clear();
    responseHeaders_->clear();
    responseBody_->clear();
    sseEvents_->clear();
    detailTitle_->clear();
    count_->setText(tr("capture.count", "Captured %1 request(s)").arg(0));
}

void CaptureDialog::exportCapture() {
    if (exchanges_.isEmpty()) {
        QMessageBox::information(this, tr("capture.export", "Export capture"),
                                 tr("capture.empty", "There are no captured exchanges to export."));
        return;
    }
    QString selected;
    const QString filter = QStringLiteral("HAR / JSON (*.har *.json);;Markdown (*.md);;JSON (*.json)");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("capture.export", "Export capture"),
        QStringLiteral("ai-relay-capture.har"), filter, &selected);
    if (path.isEmpty()) return;

    QByteArray data;
    if (selected.startsWith(QStringLiteral("Markdown")) || path.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) {
        data = HarExporter::toMarkdown(exchanges_).toUtf8();
    } else if (selected.startsWith(QStringLiteral("JSON")) && !selected.startsWith(QStringLiteral("HAR"))) {
        QJsonArray array;
        for (const auto& exchange : exchanges_)
            array.append(captureExchangeToJson(exchange, true));
        data = QJsonDocument(array).toJson(QJsonDocument::Indented);
    } else {
        data = HarExporter::toJson(exchanges_);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
        QMessageBox::critical(this, tr("capture.export", "Export capture"), file.errorString());
        return;
    }
    state_->setText(tr("capture.exported", "Exported: ") + path);
}

void CaptureDialog::updateState(bool running, const QString& message) {
    start_->setEnabled(!running);
    stop_->setEnabled(running);
    address_->setEnabled(!running);
    port_->setEnabled(!running);
    if (!message.isEmpty()) {
        state_->setText(message);
    } else {
        state_->setText(running ? tr("capture.running", "Capture proxy is running")
                                : tr("capture.stopped", "Capture proxy is stopped"));
    }
    environment_->setText(tr("capture.environment",
                              "HTTP proxy only; HTTPS CONNECT is not enabled. CLI settings:\n"
                              "$env:HTTP_PROXY=\"http://127.0.0.1:%1\"\n"
                              "$env:HTTPS_PROXY=\"http://127.0.0.1:%1\"")
                           .arg(port_->value()));
}

void CaptureDialog::retranslate() {
    const QString title = tr("capture.analysis", "Capture Analysis");
    setWindowTitle(title);
    title_->setText(title);
    if (titleBar_) titleBar_->setTitle(title);
    if (auto* label = findChild<QLabel*>(QStringLiteral("captureAddressLabel")))
        label->setText(tr("capture.address", "Listen address"));
    if (auto* label = findChild<QLabel*>(QStringLiteral("capturePortLabel")))
        label->setText(tr("capture.port", "Port"));
    start_->setText(tr("capture.start", "Start capture"));
    stop_->setText(tr("capture.stop", "Stop capture"));
    clear_->setText(tr("capture.clear", "Clear"));
    export_->setText(tr("capture.export", "Export"));
    count_->setText(tr("capture.count", "Captured %1 request(s)").arg(exchanges_.size()));
    table_->setHorizontalHeaderLabels({
        tr("capture.time", "Time"), tr("capture.method", "Method"), tr("capture.url", "URL"),
        tr("capture.client", "Client"), tr("capture.status", "Status"),
        tr("capture.duration", "Duration ms"), tr("capture.requestBytes", "Request bytes"),
        tr("capture.responseBytes", "Response bytes")
    });
    detailTabs_->setTabText(0, tr("capture.requestHeaders", "Request headers"));
    detailTabs_->setTabText(1, tr("capture.requestBody", "Request body"));
    detailTabs_->setTabText(2, tr("capture.responseHeaders", "Response headers"));
    detailTabs_->setTabText(3, tr("capture.responseBody", "Response body"));
    detailTabs_->setTabText(4, tr("capture.sse", "SSE events"));
    updateState(proxy_->isListening());
}

void CaptureDialog::setHeaders(QPlainTextEdit* editor, const CaptureHeaders& headers) {
    editor->setPlainText(headerText(headers));
}

QString CaptureDialog::bodyText(const QByteArray& body) {
    return displayBody(body);
}

QString CaptureDialog::sseText(const QList<SseCaptureEvent>& events) {
    QStringList lines;
    for (const auto& event : events) {
        lines << QStringLiteral("event: ") + QString::fromUtf8(event.event);
        lines << QStringLiteral("id: ") + QString::fromUtf8(event.id);
        lines << QStringLiteral("data: ") + QString::fromUtf8(event.data);
        lines << QString();
    }
    return lines.join(QLatin1Char('\n'));
}

QString CaptureDialog::headerText(const CaptureHeaders& headers) {
    QStringList lines;
    for (const auto& header : headers)
        lines << QString::fromUtf8(header.name) + QStringLiteral(": ") + QString::fromUtf8(header.value);
    return lines.join(QLatin1Char('\n'));
}

} // namespace airb
