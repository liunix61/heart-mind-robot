#include "SimpleActivationWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QHostInfo>
#include <QScrollBar>
#include <QSslConfiguration>
#include <QDesktopServices>
#include <QUrl>

SimpleActivationWindow::SimpleActivationWindow(const QJsonObject& activationData, QWidget *parent)
    : QDialog(parent)
    , m_isActivated(false)
    , m_isActivating(false)
    , m_networkManager(nullptr)
    , m_currentReply(nullptr)
    , m_activationTimer(nullptr)
    , m_statusTimer(nullptr)
{
    setWindowTitle("设备激活");
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setModal(true);
    setFixedSize(400, 300);
    
    // 初始化网络管理器
    m_networkManager = new QNetworkAccessManager(this);
    
    // 初始化定时器
    m_activationTimer = new QTimer(this);
    m_activationTimer->setSingleShot(true);
    m_activationTimer->setInterval(30000); // 30秒超时
    
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(5000); // 5秒检查一次
    
    // 从激活数据中获取信息，完全参照py-xiaozhi
    m_challenge = activationData["challenge"].toString();
    m_verificationCode = activationData["code"].toString();
    QString message = activationData["message"].toString();
    
    // 从ConfigManager获取配置信息
    auto configManager = ConfigManager::getInstance();
    
    // 确保CLIENT_ID已初始化
    configManager->initializeClientId();
    m_clientId = configManager->getConfig("SYSTEM_OPTIONS.CLIENT_ID").toString();
    
    // 确保DEVICE_ID已初始化（从DeviceFingerprint获取MAC地址）
    auto deviceFingerprint = DeviceFingerprint::getInstance();
    QString macAddress = deviceFingerprint->getMacAddress();
    if (!macAddress.isEmpty()) {
        configManager->updateConfig("SYSTEM_OPTIONS.DEVICE_ID", macAddress);
        m_deviceId = macAddress;
    } else {
        m_deviceId = configManager->getConfig("SYSTEM_OPTIONS.DEVICE_ID").toString();
    }
    
    m_serverUrl = configManager->getConfig("SYSTEM_OPTIONS.NETWORK.OTA_VERSION_URL").toString();
    
    // 从DeviceFingerprint获取设备信息
    m_serialNumber = deviceFingerprint->getSerialNumber();
    m_hmacSignature = deviceFingerprint->generateHmac(m_challenge);
    
    // 生成激活码（用于显示）
    m_activationCode = m_verificationCode;
    
    // 调试信息：显示激活码来源
    qDebug() << "=== 激活码调试信息 ===";
    qDebug() << "verificationCode from activationData:" << m_verificationCode;
    qDebug() << "activationCode set to:" << m_activationCode;
    
    // 连接信号槽
    connect(m_activationTimer, &QTimer::timeout, this, &SimpleActivationWindow::onActivationTimeout);
    connect(m_statusTimer, &QTimer::timeout, this, &SimpleActivationWindow::checkActivationStatus);
    
    setupUI();
    generateDeviceInfo();
    updateUI();
    
    // 显示激活信息，不自动开始激活
    showMessage("激活码: " + m_activationCode);
    showMessage("请在浏览器中访问激活页面并输入激活码");
    
    // 启动激活流程和轮询检查
    qDebug() << "启动激活流程和轮询检查...";
    startActivation();
}

SimpleActivationWindow::~SimpleActivationWindow()
{
    if (m_currentReply) {
        m_currentReply->deleteLater();
    }
}

void SimpleActivationWindow::setupUI()
{
    // 设置窗口属性 - 参照py-xiaozhi的QML窗口尺寸
    setWindowTitle("设备激活");
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setModal(true);
    setFixedSize(520, 300);
    
    // 设置窗口样式 - 参照py-xiaozhi的ArcoDesign风格
    setStyleSheet("QDialog { background-color: #ffffff; border-radius: 10px; }");
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // // ArcoDesign 标题区域 - 完全参照py-xiaozhi的QML布局
    // QVBoxLayout* topLayout = new QVBoxLayout();
    // topLayout->setSpacing(4);
    // topLayout->setContentsMargins(0, 0, 0, 0);
    
    // // 标题
    // m_titleLabel = new QLabel("设备激活状态", this);
    // m_titleLabel->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI', 'Helvetica Neue'; font-size: 20px; font-weight: 500; color: #1d2129;");
    // topLayout->addWidget(m_titleLabel);
    
    // topLayout->addStretch();
    
    // // 激活状态显示区域 - 参照py-xiaozhi的状态指示器
    // QVBoxLayout* statusLayout = new QVBoxLayout();
    // statusLayout->setSpacing(8);
    
    // // 状态指示点
    // QLabel* statusIndicator = new QLabel("●", this);
    // statusIndicator->setStyleSheet("color: #f53f3f; font-size: 12px;");
    // statusLayout->addWidget(statusIndicator);
    
    // m_statusLabel = new QLabel("未激活", this);
    // m_statusLabel->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 12px; color: #4e5969;");
    // statusLayout->addWidget(m_statusLabel);
    
    // topLayout->addLayout(statusLayout);
    
    // 关闭按钮 - 参照py-xiaozhi的关闭按钮样式
    // QPushButton* closeButton = new QPushButton("×", this);
    // closeButton->setFixedSize(32, 32);
    // closeButton->setStyleSheet(
    //     "QPushButton { "
    //     "    background-color: transparent; "
    //     "    color: #86909c; "
    //     "    border: none; "
    //     "    border-radius: 3px; "
    //     "    font-family: Arial; "
    //     "    font-size: 18px; "
    //     "    font-weight: bold; "
    //     "} "
    //     "QPushButton:hover { "
    //     "    background-color: #ff7875; "
    //     "    color: white; "
    //     "} "
    //     "QPushButton:pressed { "
    //     "    background-color: #f53f3f; "
    //     "    color: white; "
    //     "}"
    // );
    // connect(closeButton, &QPushButton::clicked, this, &QWidget::close);
    // topLayout->addWidget(closeButton);
    
    // m_mainLayout->addLayout(topLayout);
    
    // ArcoDesign 设备信息卡片 - 参照py-xiaozhi的紧凑显示
    QWidget* deviceInfoCard = new QWidget(this);
    deviceInfoCard->setFixedHeight(140);
    deviceInfoCard->setStyleSheet(
        "QWidget { "
        "    background-color: #f7f8fa; "
        "    border-radius: 3px; "
        "    border: none; "
        "} "
        "QWidget:hover { "
        "    background-color: #f2f3f5; "
        "}"
    );
    
    QVBoxLayout* deviceInfoLayout = new QVBoxLayout(deviceInfoCard);
    deviceInfoLayout->setContentsMargins(16, 16, 16, 16);
    deviceInfoLayout->setSpacing(8);
    
    // 设备信息标题
    QLabel* deviceInfoTitle = new QLabel("设备信息", deviceInfoCard);
    deviceInfoTitle->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 13px; font-weight: 500; color: #4e5969;");
    deviceInfoLayout->addWidget(deviceInfoTitle);
    
    // 设备信息网格布局 - 参照py-xiaozhi的2列布局
    QGridLayout* deviceGridLayout = new QGridLayout();
    deviceGridLayout->setSpacing(6);
    deviceGridLayout->setHorizontalSpacing(48);
    
    // 设备序列号
    QLabel* serialLabel = new QLabel("设备序列号", deviceInfoCard);
    serialLabel->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 12px; color: #86909c;");
    deviceGridLayout->addWidget(serialLabel, 0, 0);
    
    QString serialText = m_serialNumber.isEmpty() ? "SN-7B46DAF2-00ff732a9678" : m_serialNumber;
    QLabel* serialValue = new QLabel(serialText, deviceInfoCard);
    serialValue->setStyleSheet("font-family: 'SF Mono', 'Consolas', monospace; font-size: 12px; color: #1d2129;");
    deviceGridLayout->addWidget(serialValue, 1, 0);
    
    // MAC地址
    QLabel* macLabel = new QLabel("MAC地址", deviceInfoCard);
    macLabel->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 12px; color: #86909c;");
    deviceGridLayout->addWidget(macLabel, 0, 1);
    
    QString macText = m_deviceId.isEmpty() ? "00:ff:73:2a:96:78" : m_deviceId;
    QLabel* macValue = new QLabel(macText, deviceInfoCard);
    macValue->setStyleSheet("font-family: 'SF Mono', 'Consolas', monospace; font-size: 12px; color: #1d2129;");
    deviceGridLayout->addWidget(macValue, 1, 1);
    
    // 激活状态
    QLabel* statusLabel = new QLabel("激活状态", deviceInfoCard);
    statusLabel->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 12px; color: #86909c;");
    deviceGridLayout->addWidget(statusLabel, 2, 0);
    
    m_statusLabel = new QLabel("未激活", deviceInfoCard);
    m_statusLabel->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 12px; color: #f53f3f;");
    deviceGridLayout->addWidget(m_statusLabel, 3, 0);
    
    deviceInfoLayout->addLayout(deviceGridLayout);
    m_mainLayout->addWidget(deviceInfoCard);
    
    // ArcoDesign 激活验证码卡片 - 参照py-xiaozhi的一行显示
    QWidget* codeCard = new QWidget(this);
    codeCard->setFixedHeight(64);
    codeCard->setStyleSheet(
        "QWidget { "
        "    background-color: #f7f8fa; "
        "    border-radius: 3px; "
        "    border: none; "
        "} "
        "QWidget:hover { "
        "    background-color: #f2f3f5; "
        "}"
    );
    
    QHBoxLayout* codeLayout = new QHBoxLayout(codeCard);
    codeLayout->setContentsMargins(16, 16, 16, 16);
    codeLayout->setSpacing(16);
    
    // 激活验证码标签
    QLabel* codeTitle = new QLabel("激活验证码", codeCard);
    codeTitle->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 13px; font-weight: 500; color: #4e5969;");
    codeLayout->addWidget(codeTitle);
    
    // 验证码显示区域 - 参照py-xiaozhi的白色背景框
    QWidget* codeDisplayWidget = new QWidget(codeCard);
    codeDisplayWidget->setFixedHeight(36);
    codeDisplayWidget->setStyleSheet(
        "QWidget { "
        "    background-color: #ffffff; "
        "    border: 1px solid #e5e6eb; "
        "    border-radius: 3px; "
        "}"
    );
    codeLayout->addWidget(codeDisplayWidget, 1); // 占据剩余空间
    
    QHBoxLayout* codeDisplayLayout = new QHBoxLayout(codeDisplayWidget);
    codeDisplayLayout->setContentsMargins(12, 0, 12, 0);
    
    m_activationCodeEdit = new QLineEdit(m_activationCode, codeDisplayWidget);
    m_activationCodeEdit->setReadOnly(true);
    m_activationCodeEdit->setStyleSheet(
        "QLineEdit { "
        "    font-family: 'SF Mono', 'Consolas', monospace; "
        "    font-size: 15px; "
        "    font-weight: 500; "
        "    color: #f53f3f; "
        "    letter-spacing: 2px; "
        "    border: none; "
        "    background-color: transparent; "
        "    text-align: center; "
        "}"
    );
    codeDisplayLayout->addWidget(m_activationCodeEdit);
    
    // 复制按钮 - 参照py-xiaozhi的蓝色按钮样式
    m_copyButton = new QPushButton("复制", codeCard);
    m_copyButton->setFixedSize(80, 36);
    m_copyButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #165dff; "
        "    color: white; "
        "    border: none; "
        "    border-radius: 3px; "
        "    font-family: 'PingFang SC', 'Microsoft YaHei UI'; "
        "    font-size: 13px; "
        "    font-weight: 500; "
        "} "
        "QPushButton:hover { "
        "    background-color: #4080ff; "
        "} "
        "QPushButton:pressed { "
        "    background-color: #0e42d2; "
        "}"
    );
    connect(m_copyButton, &QPushButton::clicked, this, &SimpleActivationWindow::onCopyCodeClicked);
    codeLayout->addWidget(m_copyButton);
    
    m_mainLayout->addWidget(codeCard);
    
    // ArcoDesign 按钮区域 - 参照py-xiaozhi的跳转激活按钮
    QPushButton* jumpButton = new QPushButton("跳转激活", this);
    jumpButton->setFixedHeight(36);
    jumpButton->setStyleSheet(
        "QPushButton { "
        "    background-color: #165dff; "
        "    color: white; "
        "    border: none; "
        "    border-radius: 3px; "
        "    font-family: 'PingFang SC', 'Microsoft YaHei UI'; "
        "    font-size: 14px; "
        "    font-weight: 500; "
        "} "
        "QPushButton:hover { "
        "    background-color: #4080ff; "
        "} "
        "QPushButton:pressed { "
        "    background-color: #0e42d2; "
        "}"
    );
    connect(jumpButton, &QPushButton::clicked, this, &SimpleActivationWindow::onJumpToActivationClicked);
    m_mainLayout->addWidget(jumpButton);
    
    // 日志区域（隐藏，仅用于调试）
    m_logText = new QTextEdit(this);
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(60);
    m_logText->setStyleSheet("QTextEdit { background-color: #f8f9fa; border: 1px solid #dee2e6; border-radius: 4px; padding: 4px; font-size: 10px; }");
    m_logText->setVisible(false); // 隐藏日志区域
    m_mainLayout->addWidget(m_logText);
}

void SimpleActivationWindow::updateUI()
{
    if (!m_statusLabel || !m_activationCodeEdit || !m_copyButton) {
        qDebug() << "UI components not initialized, skipping updateUI";
        return;
    }
    
    if (m_isActivated) {
        m_statusLabel->setText("已激活");
        m_statusLabel->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 12px; color: #00b42a;");
        m_activationCodeEdit->setVisible(false);
        m_copyButton->setVisible(false);
    } else {
        m_statusLabel->setText("未激活");
        m_statusLabel->setStyleSheet("font-family: 'PingFang SC', 'Microsoft YaHei UI'; font-size: 12px; color: #f53f3f;");
        m_activationCodeEdit->setVisible(true);
        m_copyButton->setVisible(true);
    }
}

void SimpleActivationWindow::generateDeviceInfo()
{
    // 验证码应该来自服务端的激活数据，不要重新生成
    // m_activationCode已经在构造函数中从服务端激活数据获取了
    if (m_activationCodeEdit) {
        m_activationCodeEdit->setText(m_activationCode);
    }
    
    qDebug() << "Device ID:" << m_deviceId;
    qDebug() << "Activation code from server:" << m_activationCode;
}

void SimpleActivationWindow::startActivation()
{
    if (m_isActivating || m_isActivated) {
        return;
    }
    
    startActivationProcess();
}

void SimpleActivationWindow::startActivationProcess()
{
    qDebug() << "=== 开始激活流程 ===";
    qDebug() << "当前激活状态 - isActivating:" << m_isActivating << ", isActivated:" << m_isActivated;
    
    m_isActivating = true;
    updateUI();
    
    // 显示激活码
    if (m_activationCodeEdit) {
        m_activationCodeEdit->setText(m_activationCode);
        m_activationCodeEdit->setVisible(true);
    }
    if (m_copyButton) {
        m_copyButton->setVisible(true);
    }
    
    showMessage("激活码: " + m_activationCode);
    showMessage("请在浏览器中访问激活页面并输入激活码");
    
    // 发送激活请求
    qDebug() << "发送首次激活请求...";
    sendActivationRequest();
    
    // 移除超时限制，允许用户一直等待直到激活成功
    // 用户可以随时关闭窗口退出
    // qDebug() << "启动激活超时定时器 (30秒)";
    // m_activationTimer->start();
    
    // 启动状态检查定时器
    qDebug() << "启动状态检查定时器 (5秒间隔)";
    m_statusTimer->start();
    
    qDebug() << "=== 激活流程启动完成 ===";
}

void SimpleActivationWindow::checkActivationStatus()
{
    qDebug() << "=== 检查激活状态 ===";
    qDebug() << "当前状态 - isActivating:" << m_isActivating << ", isActivated:" << m_isActivated;
    
    if (!m_isActivating) {
        qDebug() << "激活流程未启动，跳过状态检查";
        return;
    }
    
    qDebug() << "重新发送激活请求来检查状态...";
    // 完全参照py-xiaozhi，通过重新发送激活请求来检查状态
    sendActivationRequest();
}

void SimpleActivationWindow::sendActivationRequest()
{
    if (m_currentReply) {
        // 断开信号连接，避免abort触发finished信号导致问题
        m_currentReply->disconnect(this);
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    // 使用OTA URL + "activate"端点，完全参照py-xiaozhi
    QString activateUrl = m_serverUrl;
    if (!activateUrl.endsWith("/")) {
        activateUrl += "/";
    }
    activateUrl += "activate";
    
    QNetworkRequest request(activateUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 强制使用HTTP/1.1，避免HTTP/2协议问题
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    
    // 设置请求头，完全参照py-xiaozhi
    request.setRawHeader("Activation-Version", "2");
    request.setRawHeader("Device-Id", m_deviceId.toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    request.setRawHeader("User-Agent", "bread-compact-wifi/desktop-pet-1.0.0");
    request.setRawHeader("Accept-Language", "zh-CN");
    
    // 构建激活请求体，完全参照py-xiaozhi的格式
    QJsonObject payload;
    QJsonObject payloadData;
    payloadData["algorithm"] = "hmac-sha256";
    payloadData["serial_number"] = m_serialNumber;
    payloadData["challenge"] = m_challenge;
    payloadData["hmac"] = m_hmacSignature;
    payload["Payload"] = payloadData;
    
    m_currentReply = m_networkManager->post(request, QJsonDocument(payload).toJson());
    connect(m_currentReply, &QNetworkReply::finished, this, &SimpleActivationWindow::onNetworkReplyFinished);
    
    showMessage("发送激活请求到服务器...");
    qDebug() << "=== 激活请求详细信息 ===";
    qDebug() << "请求URL:" << activateUrl;
    qDebug() << "请求头信息:";
    qDebug() << "  Activation-Version: 2";
    qDebug() << "  Device-Id:" << m_deviceId;
    qDebug() << "  Client-Id:" << m_clientId;
    qDebug() << "  User-Agent: bread-compact-wifi/desktop-pet-1.0.0";
    qDebug() << "  Accept-Language: zh-CN";
    qDebug() << "请求体数据:";
    qDebug() << "  algorithm: hmac-sha256";
    qDebug() << "  serial_number:" << m_serialNumber;
    qDebug() << "  challenge:" << m_challenge;
    qDebug() << "  hmac:" << m_hmacSignature;
    qDebug() << "完整Payload:" << QJsonDocument(payload).toJson(QJsonDocument::Compact);
    qDebug() << "=== 激活请求发送完成 ===";
}

void SimpleActivationWindow::sendStatusCheckRequest()
{
    // 完全参照py-xiaozhi，不调用/status接口，而是重新发送激活请求
    // py-xiaozhi通过重复发送激活请求来检查状态
    sendActivationRequest();
}

void SimpleActivationWindow::onJumpToActivationClicked()
{
    // 跳转到激活页面
    QString activationUrl = "https://xiaozhi.me/";
    QDesktopServices::openUrl(QUrl(activationUrl));
    qDebug() << "Opening activation page:" << activationUrl;
}

void SimpleActivationWindow::onCancelClicked()
{
    // 简化关闭逻辑，直接关闭窗口
    emit activationCancelled();
    close();
}

void SimpleActivationWindow::onCopyCodeClicked()
{
    if (!m_activationCode.isEmpty()) {
        QClipboard* clipboard = QApplication::clipboard();
        if (clipboard) {
            clipboard->setText(m_activationCode);
            showMessage("激活码已复制到剪贴板");
        } else {
            qDebug() << "Clipboard is not available";
        }
    }
}

void SimpleActivationWindow::onNetworkReplyFinished()
{
    if (!m_currentReply) {
        qDebug() << "Network reply is null, ignoring";
        return;
    }
    
    // 先检查网络错误，避免在错误状态下访问属性
    if (m_currentReply->error() != QNetworkReply::NoError) {
        qDebug() << "Network error:" << m_currentReply->errorString();
        showMessage("网络错误: " + m_currentReply->errorString());
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        m_statusTimer->start(10000); // 10秒后重试
        return;
    }
    
    QByteArray responseData = m_currentReply->readAll();
    int statusCode = m_currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    qDebug() << "=== 服务器响应详细信息 ===";
    qDebug() << "HTTP状态码:" << statusCode;
    qDebug() << "响应数据:" << responseData;
    qDebug() << "响应数据(字符串):" << QString::fromUtf8(responseData);
    showMessage(QString("服务器响应 (HTTP %1): %2").arg(statusCode).arg(QString::fromUtf8(responseData)));
    
    // 解析响应数据
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    if (!responseDoc.isNull()) {
        QJsonObject responseObj = responseDoc.object();
        qDebug() << "解析后的响应JSON:" << responseObj;
        
        if (responseObj.contains("error")) {
            qDebug() << "服务器错误信息:" << responseObj["error"].toString();
        }
        if (responseObj.contains("message")) {
            qDebug() << "服务器消息:" << responseObj["message"].toString();
        }
        if (responseObj.contains("status")) {
            qDebug() << "激活状态:" << responseObj["status"].toString();
        }
    }
    
    // 添加更详细的调试信息
    qDebug() << "=== 当前设备信息 ===";
    qDebug() << "Device ID:" << m_deviceId;
    qDebug() << "Client ID:" << m_clientId;
    qDebug() << "Serial Number:" << m_serialNumber;
    qDebug() << "Activation Code:" << m_activationCode;
    qDebug() << "Challenge:" << m_challenge;
    qDebug() << "HMAC Signature:" << m_hmacSignature;
    qDebug() << "=== 响应处理开始 ===";
    
    // 完全参照py-xiaozhi的响应处理逻辑
    if (statusCode == 200) {
        // 激活成功
        qDebug() << "Device activation successful!";
        m_isActivated = true;
        m_isActivating = false;
        m_activationTimer->stop();
        m_statusTimer->stop();
        
        qDebug() << "Calling saveActivationConfig()...";
        saveActivationConfig();
        qDebug() << "saveActivationConfig() completed";
        
        updateUI();
        showMessage("设备激活成功！");
        
        qDebug() << "Closing activation window in 2 seconds...";
        // 确保激活状态已保存，然后关闭窗口
        QTimer::singleShot(2000, this, [this]() {
            qDebug() << "Activation window closing, emitting activationCompleted signal";
            emit activationCompleted(true);
            this->accept(); // 使用accept()而不是close()
        });
    } else if (statusCode == 202) {
        // 等待用户输入验证码，继续轮询
        qDebug() << "Waiting for user to enter verification code, continuing to wait...";
        showMessage("等待用户输入验证码，继续等待...");
        m_statusTimer->start(5000); // 5秒后再次检查
    } else {
        // 处理其他错误但继续重试
        QString errorMsg = "未知错误";
        try {
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            QJsonObject response = doc.object();
            errorMsg = response["error"].toString();
            if (errorMsg.isEmpty()) {
                errorMsg = QString("未知错误 (状态码: %1)").arg(statusCode);
            }
        } catch (...) {
            errorMsg = QString("服务器返回错误 (状态码: %1)").arg(statusCode);
        }
        
        qDebug() << "Server returned:" << errorMsg << "，继续等待验证码激活";
        showMessage(QString("服务器返回: %1，继续等待验证码激活").arg(errorMsg));
        m_statusTimer->start(5000); // 5秒后重试
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void SimpleActivationWindow::onActivationTimeout()
{
    // 已移除超时限制，此函数保留以防需要重新启用
    // 如果需要超时提示但不中断激活检查，可以只显示消息而不停止定时器
    qDebug() << "Activation timeout (disabled)";
    // 不再停止状态检查，让用户可以一直等待直到激活成功
    // m_isActivating = false;
    // m_statusTimer->stop();
    // updateUI();
    // showMessage("激活超时，请检查网络连接");
}

void SimpleActivationWindow::showMessage(const QString& message)
{
    if (!m_logText) {
        qDebug() << "m_logText is null, cannot show message:" << message;
        return;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logText->append(timestamp + " - " + message);
    
    // 安全地访问滚动条
    QScrollBar* scrollBar = m_logText->verticalScrollBar();
    if (scrollBar) {
        scrollBar->setValue(scrollBar->maximum());
    }
}

void SimpleActivationWindow::saveActivationConfig()
{
    // 激活状态现在由DeviceFingerprint管理
    DeviceFingerprint::getInstance()->setActivationStatus(true);
    qDebug() << "Device activation status updated to true";
}

void SimpleActivationWindow::loadActivationConfig()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QString configPath = QDir(configDir).absoluteFilePath("activation.json");
    QFile configFile(configPath);
    
    if (configFile.exists() && configFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
        m_config = doc.object();
        
        if (m_config.contains("activated") && m_config["activated"].toBool()) {
            m_isActivated = true;
            if (m_config.contains("device_id")) {
                m_deviceId = m_config["device_id"].toString();
            }
            qDebug() << "Device already activated";
        }
    }
}
