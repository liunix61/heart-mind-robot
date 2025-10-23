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
    , m_heartbeatInterval(30000) // 30秒
    , m_pongTimeout(10000) // 10秒
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
            qWarning() << "Pong timeout, connection may be lost";
            emit connectionError("Heartbeat timeout");
        }
    });
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
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->close();
    }
    stopHeartbeat();
    m_connected = false;
    setCurrentState(DeviceState::DISCONNECTED);
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
        m_heartbeatTimer->start(m_heartbeatInterval);
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
    
    emit disconnected();
}

void WebSocketManager::onTextMessageReceived(const QString &message)
{
    qDebug() << "Received text message:" << message;
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
        m_pongReceived = false;
        m_webSocket->ping();
        m_pongTimer->start(m_pongTimeout);
    }
}

void WebSocketManager::onPongReceived(quint64 elapsedTime, const QByteArray &payload)
{
    Q_UNUSED(elapsedTime)
    Q_UNUSED(payload)
    
    m_pongReceived = true;
    m_pongTimer->stop();
    qDebug() << "Pong received, elapsed time:" << elapsedTime << "ms";
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
    else if (typeStr == "ping") message.type = MessageType::PING;
    else if (typeStr == "pong") message.type = MessageType::PONG;
    else message.type = MessageType::HELLO; // 默认
    
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
        case MessageType::PING: return "ping";
        case MessageType::PONG: return "pong";
        default: return "hello";
        }
    }();
    
    json["session_id"] = message.sessionId;
    json["timestamp"] = message.timestamp;
    
    // 合并数据
    for (auto it = message.data.begin(); it != message.data.end(); ++it) {
        json[it.key()] = it.value();
    }
    
    QJsonDocument doc(json);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

void WebSocketManager::handleHelloResponse(const QJsonObject &data)
{
    qDebug() << "Received hello response from server";
    setCurrentState(DeviceState::IDLE);
}

void WebSocketManager::handleTTSMessage(const QJsonObject &data)
{
    QString text = data["text"].toString();
    QString emotion = data["emotion"].toString();
    QString state = data["state"].toString();
    
    if (state == "start") {
        setCurrentState(DeviceState::SPEAKING);
    } else if (state == "stop") {
        setCurrentState(DeviceState::IDLE);
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
    qDebug() << "Received pong from server";
}

QString WebSocketManager::generateSessionId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString WebSocketManager::getCurrentTimestamp()
{
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}
