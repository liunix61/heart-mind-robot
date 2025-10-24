#include "WebSocketManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QDebug>
#include <QThread>
#include <QMutexLocker>

WebSocketManager::WebSocketManager(QObject *parent)
    : QObject(parent)
    , m_webSocket(nullptr)
    , m_connected(false)
    , m_heartbeatTimer(nullptr)
    , m_pongTimer(nullptr)
    , m_pongReceived(true)
    , m_heartbeatInterval(20000) // 20秒 - 与py-xiaozhi保持一致
    , m_pongTimeout(20000) // 20秒 - 与py-xiaozhi保持一致
    , m_reconnectTimer(nullptr)
    , m_reconnectInterval(3000) // 3秒后重连（更快）
    , m_reconnectAttempts(0)
    , m_maxReconnectAttempts(999) // 几乎无限重连
    , m_currentState(DeviceState::DISCONNECTED)
    , m_protocolVersion("1")
{
    initializeWebSocket();
}

WebSocketManager::~WebSocketManager()
{
    disconnectFromServer();
    if (m_webSocket) {
        m_webSocket->deleteLater();
    }
    if (m_heartbeatTimer) {
        m_heartbeatTimer->deleteLater();
    }
    if (m_pongTimer) {
        m_pongTimer->deleteLater();
    }
    if (m_reconnectTimer) {
        m_reconnectTimer->deleteLater();
    }
}

void WebSocketManager::initializeWebSocket()
{
    if (m_webSocket) {
        m_webSocket->deleteLater();
    }
    
    m_webSocket = new QWebSocket();
    
    // 连接信号
    connect(m_webSocket, &QWebSocket::connected, this, &WebSocketManager::onConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &WebSocketManager::onDisconnected);
    connect(m_webSocket, &QWebSocket::textMessageReceived, this, &WebSocketManager::onTextMessageReceived);
    connect(m_webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketManager::onBinaryMessageReceived);
    connect(m_webSocket, &QWebSocket::errorOccurred, 
            this, &WebSocketManager::onError);
    connect(m_webSocket, &QWebSocket::pong, this, &WebSocketManager::onPongReceived);
    
    // 初始化心跳定时器
    m_heartbeatTimer = new QTimer(this);
    m_pongTimer = new QTimer(this);
    m_pongTimer->setSingleShot(true);
    
    connect(m_heartbeatTimer, &QTimer::timeout, this, &WebSocketManager::onHeartbeatTimeout);
    connect(m_pongTimer, &QTimer::timeout, [this]() {
        if (!m_pongReceived) {
            qWarning() << "======================================";
            qWarning() << "心跳超时 - 没有收到服务器的pong响应";
            qWarning() << "超时时间:" << m_pongTimeout << "ms (" << (m_pongTimeout/1000) << "秒)";
            qWarning() << "WebSocket状态:" << m_webSocket->state();
            qWarning() << "======================================";
            
            // 立即停止心跳，避免在关闭过程中再次触发
            stopHeartbeat();
            
            emit connectionError("心跳超时，连接可能已断开");
            
            // 关闭连接以触发重连
            if (m_webSocket) {
                qDebug() << "关闭WebSocket连接以触发重连...";
                m_webSocket->close();
            }
        } else {
            qDebug() << "✓ Pong received on time, connection is healthy";
        }
    });
    
    // 初始化重连定时器
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &WebSocketManager::onReconnectTimeout);
}

bool WebSocketManager::connectToServer(const QString &url, const QString &accessToken)
{
    if (m_connected) {
        qWarning() << "Already connected to server";
        return true;
    }
    
    m_serverUrl = QUrl(url);
    m_accessToken = accessToken;
    
    if (!m_serverUrl.isValid()) {
        qCritical() << "Invalid server URL:" << url;
        emit connectionError("Invalid server URL");
        return false;
    }
    
    qDebug() << "Connecting to WebSocket server:" << url;
    
    // 设置请求头
    QNetworkRequest request(m_serverUrl);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(accessToken).toUtf8());
    request.setRawHeader("Protocol-Version", m_protocolVersion.toUtf8());
    request.setRawHeader("Device-Id", m_deviceId.toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    
    m_webSocket->open(request);
    return true;
}

void WebSocketManager::disconnectFromServer()
{
    qDebug() << "Disconnecting from server...";
    
    // 先停止心跳，再关闭连接
    stopHeartbeat();
    m_connected = false;
    setCurrentState(DeviceState::DISCONNECTED);
    
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->close();
    }
}

bool WebSocketManager::isConnected() const
{
    return m_connected && m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState;
}

void WebSocketManager::sendHello()
{
    QJsonObject helloData;
    helloData["version"] = 1;
    helloData["features"] = QJsonObject{
        {"mcp", true}
    };
    helloData["transport"] = "websocket";
    helloData["audio_params"] = QJsonObject{
        {"format", "opus"},
        {"sample_rate", 16000},
        {"channels", 1},
        {"frame_duration", 20}
    };
    
    WebSocketMessage message;
    message.type = MessageType::HELLO;
    message.data = helloData;
    message.sessionId = m_sessionId;
    message.timestamp = getCurrentTimestamp();
    
    sendMessage(message);
}

void WebSocketManager::sendListenStart()
{
    QJsonObject listenData;
    listenData["state"] = "start";
    listenData["mode"] = "manual";
    
    WebSocketMessage message;
    message.type = MessageType::LISTEN;
    message.data = listenData;
    message.sessionId = m_sessionId;
    message.timestamp = getCurrentTimestamp();
    
    sendMessage(message);
}

void WebSocketManager::sendListenStop()
{
    QJsonObject listenData;
    listenData["state"] = "stop";
    
    WebSocketMessage message;
    message.type = MessageType::LISTEN;
    message.data = listenData;
    message.sessionId = m_sessionId;
    message.timestamp = getCurrentTimestamp();
    
    sendMessage(message);
}

void WebSocketManager::sendAbortSpeaking()
{
    QJsonObject abortData;
    abortData["reason"] = "user_interruption";
    
    WebSocketMessage message;
    message.type = MessageType::ABORT;
    message.data = abortData;
    message.sessionId = m_sessionId;
    message.timestamp = getCurrentTimestamp();
    
    sendMessage(message);
}

void WebSocketManager::sendWakeWordDetected(const QString &text)
{
    QJsonObject wakeData;
    wakeData["state"] = "detect";
    wakeData["text"] = text;
    
    WebSocketMessage message;
    message.type = MessageType::LISTEN;
    message.data = wakeData;
    message.sessionId = m_sessionId;
    message.timestamp = getCurrentTimestamp();
    
    sendMessage(message);
}

void WebSocketManager::sendAudioData(const QByteArray &audioData)
{
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->sendBinaryMessage(audioData);
    }
}

DeviceState WebSocketManager::getCurrentState() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_currentState;
}

void WebSocketManager::setCurrentState(DeviceState state)
{
    {
        QMutexLocker locker(&m_stateMutex);
        if (m_currentState == state) return;
        m_currentState = state;
    }
    
    qDebug() << "Device state changed to:" << static_cast<int>(state);
    emit stateChanged(state);
}

void WebSocketManager::setDeviceId(const QString &deviceId)
{
    m_deviceId = deviceId;
}

void WebSocketManager::setClientId(const QString &clientId)
{
    m_clientId = clientId;
}

void WebSocketManager::setAccessToken(const QString &token)
{
    m_accessToken = token;
}

void WebSocketManager::startHeartbeat()
{
    if (m_heartbeatTimer) {
        qDebug() << "Starting heartbeat timer with interval:" << m_heartbeatInterval << "ms";
        m_heartbeatTimer->start(m_heartbeatInterval);
    } else {
        qCritical() << "Heartbeat timer is null!";
    }
}

void WebSocketManager::stopHeartbeat()
{
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
    if (m_pongTimer) {
        m_pongTimer->stop();
    }
}

void WebSocketManager::onConnected()
{
    qDebug() << "WebSocket connected successfully";
    m_connected = true;
    m_sessionId = generateSessionId();
    setCurrentState(DeviceState::CONNECTING);
    
    // 重置重连计数
    m_reconnectAttempts = 0;
    stopReconnect();
    
    // 发送hello消息
    sendHello();
    
    // 开始心跳
    startHeartbeat();
    
    emit connected();
}

void WebSocketManager::onDisconnected()
{
    qDebug() << "WebSocket disconnected";
    qDebug() << "Disconnect reason - State:" << m_webSocket->state() << "Error:" << m_webSocket->errorString();
    m_connected = false;
    setCurrentState(DeviceState::DISCONNECTED);
    stopHeartbeat();
    
    // 启动自动重连
    qDebug() << "Will attempt to reconnect in" << m_reconnectInterval << "ms";
    startReconnect();
    
    emit disconnected();
}

void WebSocketManager::onTextMessageReceived(const QString &message)
{
    qDebug() << "========================================";
    qDebug() << "=== Raw WebSocket Text Message ===";
    qDebug() << message;
    qDebug() << "========================================";
    processIncomingMessage(message);
}

void WebSocketManager::onBinaryMessageReceived(const QByteArray &data)
{
    qDebug() << "Received binary message, size:" << data.size();
    processIncomingBinary(data);
}

void WebSocketManager::onError(QAbstractSocket::SocketError error)
{
    QString errorString = m_webSocket->errorString();
    qCritical() << "WebSocket error:" << error << errorString;
    qCritical() << "Error details - Code:" << static_cast<int>(error) << "String:" << errorString;
    qCritical() << "Connection state:" << m_webSocket->state();
    emit connectionError(QString("WebSocket error: %1").arg(errorString));
}

void WebSocketManager::onHeartbeatTimeout()
{
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        // 使用WebSocket协议层的ping（与py-xiaozhi一致）
        m_webSocket->ping();
        
        // 启动pong超时检测
        m_pongReceived = false;
        m_pongTimer->start(m_pongTimeout);
        
        qDebug() << "Heartbeat sent (WebSocket protocol ping)";
    }
}

void WebSocketManager::onPongReceived(quint64 elapsedTime, const QByteArray &payload)
{
    Q_UNUSED(payload)
    
    // WebSocket协议层的pong响应
    m_pongReceived = true;
    m_pongTimer->stop();
    qDebug() << "✓ WebSocket pong received, RTT:" << elapsedTime << "ms";
}

void WebSocketManager::processIncomingMessage(const QString &message)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse JSON message:" << error.errorString();
        return;
    }
    
    QJsonObject json = doc.object();
    WebSocketMessage wsMessage = parseMessage(json);
    
    emit messageReceived(wsMessage);
    
    // 处理特定消息类型
    switch (wsMessage.type) {
    case MessageType::HELLO:
        handleHelloResponse(wsMessage.data);
        break;
    case MessageType::TTS:
        handleTTSMessage(wsMessage.data);
        break;
    case MessageType::STT:
        handleSTTMessage(wsMessage.data);
        break;
    case MessageType::LLM:
        handleLLMMessage(wsMessage.data);
        break;
    case MessageType::IOT:
        handleIoTMessage(wsMessage.data);
        break;
    case MessageType::MCP:
        handleMCPMessage(wsMessage.data);
        break;
    case MessageType::PING:
        handlePingMessage(wsMessage.data);
        break;
    case MessageType::PONG:
        handlePongMessage(wsMessage.data);
        break;
    default:
        qDebug() << "Unknown message type received";
        break;
    }
}

void WebSocketManager::processIncomingBinary(const QByteArray &data)
{
    // 处理音频数据
    emit audioDataReceived(data);
}

WebSocketMessage WebSocketManager::parseMessage(const QJsonObject &json)
{
    WebSocketMessage message;
    
    QString typeStr = json["type"].toString();
    if (typeStr == "hello") message.type = MessageType::HELLO;
    else if (typeStr == "tts") message.type = MessageType::TTS;
    else if (typeStr == "stt") message.type = MessageType::STT;
    else if (typeStr == "llm") message.type = MessageType::LLM;
    else if (typeStr == "iot") message.type = MessageType::IOT;
    else if (typeStr == "mcp") message.type = MessageType::MCP;
    else if (typeStr == "ping") message.type = MessageType::PING;
    else if (typeStr == "pong") message.type = MessageType::PONG;
    else {
        qWarning() << "Unknown message type:" << typeStr;
        message.type = MessageType::HELLO; // 默认
    }
    
    message.data = json;
    message.sessionId = json["session_id"].toString();
    message.timestamp = json["timestamp"].toString();
    
    return message;
}

void WebSocketManager::sendMessage(const WebSocketMessage &message)
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Cannot send message: WebSocket not connected";
        return;
    }
    
    QJsonObject json;
    json["type"] = [&]() {
        switch (message.type) {
        case MessageType::HELLO: return "hello";
        case MessageType::LISTEN: return "listen";
        case MessageType::ABORT: return "abort";
        case MessageType::TTS: return "tts";
        case MessageType::STT: return "stt";
        case MessageType::LLM: return "llm";
        case MessageType::IOT: return "iot";
        case MessageType::MCP: return "mcp";
        case MessageType::PING: return "ping";
        case MessageType::PONG: return "pong";
        default: return "hello";
        }
    }();
    
    json["session_id"] = message.sessionId;
    if (!message.timestamp.isEmpty()) {
        json["timestamp"] = message.timestamp;
    }
    
    // 合并数据
    for (auto it = message.data.begin(); it != message.data.end(); ++it) {
        json[it.key()] = it.value();
    }
    
    QJsonDocument doc(json);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    
    // 调试：打印发送的消息（仅MCP类型）
    if (message.type == MessageType::MCP) {
        qDebug() << "========================================";
        qDebug() << "=== Sending MCP Response ===";
        qDebug() << jsonString;
        qDebug() << "========================================";
    }
    
    m_webSocket->sendTextMessage(jsonString);
}

void WebSocketManager::handleHelloResponse(const QJsonObject &data)
{
    qDebug() << "Received hello response from server";
    
    // 更新session_id为服务器返回的ID
    if (data.contains("session_id")) {
        m_sessionId = data["session_id"].toString();
        qDebug() << "Updated session_id from server:" << m_sessionId;
    }
    
    setCurrentState(DeviceState::IDLE);
}

void WebSocketManager::handleTTSMessage(const QJsonObject &data)
{
    QString text = data["text"].toString();
    QString emotion = data["emotion"].toString();
    QString state = data["state"].toString();
    
    // 添加调试日志 - 只在有 emotion 时显示
    if (!emotion.isEmpty()) {
        qDebug() << "========================================";
        qDebug() << "=== TTS with Emotion! ===";
        qDebug() << "Text:" << text;
        qDebug() << "Emotion:" << emotion;
        qDebug() << "State:" << state;
        qDebug() << "========================================";
    }
    
    if (state == "start") {
        setCurrentState(DeviceState::SPEAKING);
    } else if (state == "stop") {
        setCurrentState(DeviceState::IDLE);
        // 说话结束，重置表情到默认状态
        qDebug() << "TTS stopped, resetting expression to neutral";
        emit ttsMessageReceived("", "neutral");  // 发送空文本和neutral情绪来重置表情
        return;  // 直接返回，不再发送下面的信号
    }
    
    emit ttsMessageReceived(text, emotion);
}

void WebSocketManager::handleSTTMessage(const QJsonObject &data)
{
    QString text = data["text"].toString();
    emit sttMessageReceived(text);
}

void WebSocketManager::handleLLMMessage(const QJsonObject &data)
{
    QString text = data["text"].toString();
    QString emotion = data["emotion"].toString();
    
    // 添加调试日志
    qDebug() << "========================================";
    qDebug() << "=== LLM Message Received! ===";
    qDebug() << "Text:" << text;
    qDebug() << "Emotion:" << emotion;
    qDebug() << "Full data:" << data;
    qDebug() << "========================================";
    
    emit llmMessageReceived(text, emotion);
}

void WebSocketManager::handleIoTMessage(const QJsonObject &data)
{
    QJsonObject command = data["command"].toObject();
    emit iotCommandReceived(command);
}

void WebSocketManager::handlePingMessage(const QJsonObject &data)
{
    Q_UNUSED(data)
    // 自动回复pong
    QJsonObject pongData;
    WebSocketMessage message;
    message.type = MessageType::PONG;
    message.data = pongData;
    message.sessionId = m_sessionId;
    message.timestamp = getCurrentTimestamp();
    sendMessage(message);
}

void WebSocketManager::handlePongMessage(const QJsonObject &data)
{
    Q_UNUSED(data)
    // 应用层 pong 接收，重置心跳超时
    m_pongReceived = true;
    m_pongTimer->stop();
    
    // 计算pong响应时间（如果有timestamp）
    if (data.contains("timestamp")) {
        QString sentTime = data["timestamp"].toString();
        QString currentTime = getCurrentTimestamp();
        qDebug() << "✓ Received pong from server (application layer)";
        qDebug() << "  Sent:" << sentTime;
        qDebug() << "  Received:" << currentTime;
    } else {
        qDebug() << "✓ Pong received from server (application layer)";
    }
}

void WebSocketManager::handleMCPMessage(const QJsonObject &data)
{
    // MCP (Model Context Protocol) 消息处理
    qDebug() << "Received MCP message";
    
    if (!data.contains("payload")) {
        qWarning() << "MCP message missing payload";
        return;
    }
    
    QJsonObject payload = data["payload"].toObject();
    QString method = payload["method"].toString();
    int id = payload["id"].toInt();
    
    qDebug() << "MCP method:" << method << "id:" << id;
    
    // 处理不同的MCP方法
    if (method == "initialize") {
        // 初始化响应
        QJsonObject result;
        result["protocolVersion"] = "2024-11-05";
        result["capabilities"] = QJsonObject();
        result["serverInfo"] = QJsonObject{
            {"name", "heart-mind-robot"},
            {"version", "1.0.0"}
        };
        
        QJsonObject response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = result;
        
        QJsonObject mcpResponse;
        mcpResponse["payload"] = response;
        
        WebSocketMessage message;
        message.type = MessageType::MCP;
        message.data = mcpResponse;
        message.sessionId = m_sessionId;
        message.timestamp = "";  // MCP消息不需要timestamp
        
        sendMessage(message);
        qDebug() << "Sent MCP initialize response";
    }
    else if (method == "tools/list") {
        // 工具列表响应（返回空列表）
        QJsonObject result;
        result["tools"] = QJsonArray();
        
        QJsonObject response;
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = result;
        
        QJsonObject mcpResponse;
        mcpResponse["payload"] = response;
        
        WebSocketMessage message;
        message.type = MessageType::MCP;
        message.data = mcpResponse;
        message.sessionId = m_sessionId;
        message.timestamp = "";  // MCP消息不需要timestamp
        
        sendMessage(message);
        qDebug() << "Sent MCP tools/list response (empty)";
    }
    else if (method.startsWith("notifications/")) {
        // 通知类消息，不需要响应
        qDebug() << "MCP notification received (no response needed):" << method;
    }
    else {
        qWarning() << "Unknown MCP method:" << method;
    }
}

QString WebSocketManager::generateSessionId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString WebSocketManager::getCurrentTimestamp()
{
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

void WebSocketManager::onReconnectTimeout()
{
    attemptReconnect();
}

void WebSocketManager::attemptReconnect()
{
    if (m_reconnectAttempts >= m_maxReconnectAttempts) {
        qWarning() << "Max reconnect attempts reached, giving up";
        return;
    }
    
    m_reconnectAttempts++;
    qDebug() << "Attempting to reconnect... (attempt" << m_reconnectAttempts << ")";
    
    // 重新连接，使用完整的连接流程（包括设置请求头）
    if (m_webSocket && m_webSocket->state() != QAbstractSocket::ConnectedState) {
        // 设置请求头
        QNetworkRequest request(m_serverUrl);
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_accessToken).toUtf8());
        request.setRawHeader("Protocol-Version", m_protocolVersion.toUtf8());
        request.setRawHeader("Device-Id", m_deviceId.toUtf8());
        request.setRawHeader("Client-Id", m_clientId.toUtf8());
        
        m_webSocket->open(request);
    }
}

void WebSocketManager::startReconnect()
{
    if (m_reconnectTimer && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(m_reconnectInterval);
    }
}

void WebSocketManager::stopReconnect()
{
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
}
