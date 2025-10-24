//
// 对话工作线程实现
// 处理WebSocket通信和对话逻辑，避免阻塞UI线程
//

#include "ConversationWorker.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QWebSocket>
#include <QAbstractSocket>

ConversationWorker::ConversationWorker(QObject *parent)
    : QThread(parent)
    , m_running(false)
    , m_shouldStop(false)
    , m_currentState(WorkerState::IDLE)
    , m_webSocket(nullptr)
    , m_connected(false)
    , m_processingTimer(nullptr)
    , m_heartbeatTimer(nullptr)
{
    qDebug() << "ConversationWorker created";
}

ConversationWorker::~ConversationWorker()
{
    stopWorker();
    qDebug() << "ConversationWorker destroyed";
}

void ConversationWorker::startWorker()
{
    if (m_running) {
        qWarning() << "ConversationWorker already running";
        return;
    }
    
    m_running = true;
    m_shouldStop = false;
    updateState(WorkerState::IDLE);
    
    start();
    qDebug() << "ConversationWorker started";
}

void ConversationWorker::stopWorker()
{
    if (!m_running) {
        return;
    }
    
    m_shouldStop = true;
    m_running = false;
    
    // 等待线程结束
    if (isRunning()) {
        wait(5000); // 等待5秒
    }
    
    qDebug() << "ConversationWorker stopped";
}

bool ConversationWorker::isRunning() const
{
    return m_running && QThread::isRunning();
}

void ConversationWorker::processTextMessage(const QString& text)
{
    if (!m_connected) {
        qWarning() << "Not connected, cannot process text message";
        return;
    }
    
    QMutexLocker locker(&m_messageMutex);
    if (m_outgoingMessages.size() >= MAX_QUEUE_SIZE) {
        qWarning() << "Outgoing message queue full, dropping message";
        return;
    }
    
    m_outgoingMessages.enqueue(text);
    qDebug() << "Text message queued:" << text;
}

void ConversationWorker::processAudioMessage(const QByteArray& audioData)
{
    if (!m_connected) {
        qWarning() << "Not connected, cannot process audio message";
        return;
    }
    
    qDebug() << "Audio message received, size:" << audioData.size();
    // 这里可以处理音频消息，比如发送到服务器
}

void ConversationWorker::processWebSocketMessage(const QJsonObject& message)
{
    QMutexLocker locker(&m_messageMutex);
    if (m_incomingMessages.size() >= MAX_QUEUE_SIZE) {
        qWarning() << "Incoming message queue full, dropping message";
        return;
    }
    
    m_incomingMessages.enqueue(message);
    qDebug() << "WebSocket message queued";
}

void ConversationWorker::setServerUrl(const QString& url)
{
    m_serverUrl = url;
    qDebug() << "Server URL set:" << url;
}

void ConversationWorker::setAccessToken(const QString& token)
{
    m_accessToken = token;
    qDebug() << "Access token set";
}

void ConversationWorker::setDeviceId(const QString& deviceId)
{
    m_deviceId = deviceId;
    qDebug() << "Device ID set:" << deviceId;
}

void ConversationWorker::connectToServer()
{
    if (m_connected) {
        qWarning() << "Already connected";
        return;
    }
    
    updateState(WorkerState::CONNECTING);
    
    if (!m_webSocket) {
        m_webSocket = new QWebSocket();
        connect(m_webSocket, &QWebSocket::connected, this, &ConversationWorker::onWebSocketConnected);
        connect(m_webSocket, &QWebSocket::disconnected, this, &ConversationWorker::onWebSocketDisconnected);
        connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
                this, &ConversationWorker::onWebSocketError);
        connect(m_webSocket, &QWebSocket::textMessageReceived,
                this, &ConversationWorker::onWebSocketTextMessageReceived);
        connect(m_webSocket, &QWebSocket::binaryMessageReceived,
                this, &ConversationWorker::onWebSocketBinaryMessageReceived);
    }
    
    m_webSocket->open(QUrl(m_serverUrl));
    qDebug() << "Connecting to server:" << m_serverUrl;
}

void ConversationWorker::disconnectFromServer()
{
    if (m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->close();
    }
    m_connected = false;
    updateState(WorkerState::IDLE);
    qDebug() << "Disconnected from server";
}

bool ConversationWorker::isConnected() const
{
    return m_connected && m_webSocket && m_webSocket->state() == QAbstractSocket::ConnectedState;
}

WorkerState ConversationWorker::getCurrentState() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_currentState;
}

void ConversationWorker::setState(WorkerState state)
{
    updateState(state);
}

void ConversationWorker::run()
{
    qDebug() << "ConversationWorker thread started";
    
    // 初始化定时器
    initializeTimers();
    
    // 主循环
    while (m_running && !m_shouldStop) {
        // 处理消息队列
        processIncomingMessages();
        processOutgoingMessages();
        
        // 短暂休眠，避免CPU占用过高
        msleep(10);
    }
    
    // 清理资源
    if (m_webSocket) {
        m_webSocket->close();
        m_webSocket->deleteLater();
        m_webSocket = nullptr;
    }
    
    if (m_processingTimer) {
        m_processingTimer->stop();
        m_processingTimer->deleteLater();
        m_processingTimer = nullptr;
    }
    
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        m_heartbeatTimer->deleteLater();
        m_heartbeatTimer = nullptr;
    }
    
    qDebug() << "ConversationWorker thread finished";
}

void ConversationWorker::initializeTimers()
{
    // 处理超时定时器
    m_processingTimer = new QTimer();
    m_processingTimer->setSingleShot(true);
    m_processingTimer->setInterval(PROCESSING_TIMEOUT);
    connect(m_processingTimer, &QTimer::timeout, this, &ConversationWorker::onProcessingTimeout);
    
    // 心跳定时器
    m_heartbeatTimer = new QTimer();
    m_heartbeatTimer->setInterval(HEARTBEAT_INTERVAL);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ConversationWorker::onHeartbeatTimeout);
    m_heartbeatTimer->start();
    
    qDebug() << "ConversationWorker timers initialized";
}

void ConversationWorker::processIncomingMessages()
{
    QMutexLocker locker(&m_messageMutex);
    
    while (!m_incomingMessages.isEmpty()) {
        QJsonObject message = m_incomingMessages.dequeue();
        handleWebSocketMessage(message);
    }
}

void ConversationWorker::processOutgoingMessages()
{
    QMutexLocker locker(&m_messageMutex);
    
    while (!m_outgoingMessages.isEmpty()) {
        QString message = m_outgoingMessages.dequeue();
        sendTextMessage(message);
    }
}

void ConversationWorker::handleWebSocketMessage(const QJsonObject& message)
{
    QString type = message["type"].toString();
    qDebug() << "Handling WebSocket message, type:" << type;
    
    if (type == "tts") {
        QString text = message["text"].toString();
        QString emotion = message["emotion"].toString();
        handleTextMessage(text);
        if (!emotion.isEmpty()) {
            handleEmotionUpdate(emotion);
        }
    } else if (type == "stt") {
        QString text = message["text"].toString();
        handleTextMessage(text);
    } else if (type == "llm") {
        QString text = message["text"].toString();
        QString emotion = message["emotion"].toString();
        handleTextMessage(text);
        if (!emotion.isEmpty()) {
            handleEmotionUpdate(emotion);
        }
    } else if (type == "emotion") {
        QString emotion = message["emotion"].toString();
        handleEmotionUpdate(emotion);
    } else if (type == "animation") {
        QString animation = message["animation"].toString();
        handleAnimationRequest(animation);
    }
}

void ConversationWorker::handleTextMessage(const QString& text)
{
    qDebug() << "Handling text message:" << text;
    emit textMessageProcessed(text);
    emit responseReady(text);
}

void ConversationWorker::handleAudioMessage(const QByteArray& audioData)
{
    qDebug() << "Handling audio message, size:" << audioData.size();
    emit audioMessageProcessed(audioData);
    emit audioResponseReady(audioData);
}

void ConversationWorker::handleStateUpdate(const QString& state)
{
    qDebug() << "Handling state update:" << state;
    emit statusUpdateRequested(state);
}

void ConversationWorker::handleEmotionUpdate(const QString& emotion)
{
    qDebug() << "Handling emotion update:" << emotion;
    emit petEmotionChanged(emotion);
}

void ConversationWorker::handleAnimationRequest(const QString& animation)
{
    qDebug() << "Handling animation request:" << animation;
    emit petAnimationRequested(animation);
}

void ConversationWorker::sendTextMessage(const QString& text)
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "WebSocket not connected, cannot send message";
        return;
    }
    
    QJsonObject message;
    message["type"] = "stt";
    message["text"] = text;
    message["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    QJsonDocument doc(message);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
    qDebug() << "Text message sent:" << text;
}

void ConversationWorker::sendAudioMessage(const QByteArray& audioData)
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "WebSocket not connected, cannot send audio";
        return;
    }
    
    m_webSocket->sendBinaryMessage(audioData);
    qDebug() << "Audio message sent, size:" << audioData.size();
}

void ConversationWorker::sendHeartbeat()
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    QJsonObject message;
    message["type"] = "ping";
    message["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    QJsonDocument doc(message);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
    qDebug() << "Heartbeat sent";
}

void ConversationWorker::sendConnectionRequest()
{
    if (!m_webSocket || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    
    QJsonObject message;
    message["type"] = "hello";
    message["access_token"] = m_accessToken;
    message["device_id"] = m_deviceId;
    message["client_id"] = m_clientId;
    message["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    QJsonDocument doc(message);
    m_webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
    qDebug() << "Connection request sent";
}

void ConversationWorker::handleError(const QString& error)
{
    qCritical() << "ConversationWorker error:" << error;
    updateState(WorkerState::ERROR);
    emit errorOccurred(error);
}

void ConversationWorker::handleProcessingError(const QString& error)
{
    qWarning() << "Processing error:" << error;
    emit processingError(error);
}

void ConversationWorker::updateState(WorkerState newState)
{
    QMutexLocker locker(&m_stateMutex);
    if (m_currentState != newState) {
        m_currentState = newState;
        notifyStateChange(newState);
    }
}

void ConversationWorker::notifyStateChange(WorkerState newState)
{
    emit stateChanged(newState);
    qDebug() << "Worker state changed to:" << static_cast<int>(newState);
}

// 槽函数实现
void ConversationWorker::onWebSocketConnected()
{
    qDebug() << "WebSocket connected";
    m_connected = true;
    updateState(WorkerState::CONNECTED);
    sendConnectionRequest();
    emit connected();
}

void ConversationWorker::onWebSocketDisconnected()
{
    qDebug() << "WebSocket disconnected";
    m_connected = false;
    updateState(WorkerState::IDLE);
    emit disconnected();
}

void ConversationWorker::onWebSocketError(QAbstractSocket::SocketError error)
{
    QString errorString = QString("WebSocket error: %1").arg(error);
    qCritical() << errorString;
    m_connected = false;
    updateState(WorkerState::ERROR);
    emit connectionError(errorString);
}

void ConversationWorker::onWebSocketTextMessageReceived(const QString& message)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse WebSocket message:" << error.errorString();
        return;
    }
    
    QJsonObject json = doc.object();
    processWebSocketMessage(json);
}

void ConversationWorker::onWebSocketBinaryMessageReceived(const QByteArray& message)
{
    qDebug() << "Binary message received, size:" << message.size();
    handleAudioMessage(message);
}

void ConversationWorker::onProcessingTimeout()
{
    qWarning() << "Processing timeout";
    handleProcessingError("Processing timeout");
}

void ConversationWorker::onHeartbeatTimeout()
{
    sendHeartbeat();
}

