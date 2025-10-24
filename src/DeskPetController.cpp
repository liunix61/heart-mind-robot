#include "DeskPetController.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QAudioFormat>
#include <QBuffer>
#include <QThread>

DeskPetController::DeskPetController(QObject *parent)
    : QObject(parent)
    , m_webSocketManager(nullptr)
    , m_stateManager(nullptr)
    , m_configManager(nullptr)
    , m_live2DManager(nullptr)
    , m_audioBuffer(nullptr)
    , m_networkManager(nullptr)
    , m_heartbeatTimer(nullptr)
    , m_statusUpdateTimer(nullptr)
    , m_initialized(false)
    , m_audioEnabled(true)
    , m_microphoneEnabled(true)
    , m_speakerEnabled(true)
    , m_animationEnabled(true)
{
    initializeComponents();
}

DeskPetController::~DeskPetController()
{
    shutdown();
}

bool DeskPetController::initialize()
{
    if (m_initialized) {
        qWarning() << "DeskPetController already initialized";
        return true;
    }
    
    qDebug() << "Initializing DeskPetController...";
    
    try {
        // 初始化配置管理器
        m_configManager = ConfigManager::getInstance();
        loadConfiguration();
        
        // 初始化WebSocket管理器
        m_webSocketManager = new WebSocketManager(this);
        
        // 初始化状态管理器
        m_stateManager = new DeskPetStateManager(this);
        
        // 初始化网络管理器
        m_networkManager = new QNetworkAccessManager(this);
        
        // 设置音频配置
        setupAudio();
        
        // 设置定时器
        setupTimers();
        
        // 建立连接
        setupConnections();
        
        m_initialized = true;
        qDebug() << "DeskPetController initialized successfully";
        return true;
        
    } catch (const std::exception &e) {
        qCritical() << "Failed to initialize DeskPetController:" << e.what();
        return false;
    }
}

void DeskPetController::shutdown()
{
    if (!m_initialized) return;
    
    qDebug() << "Shutting down DeskPetController...";
    
    // 断开连接
    disconnectFromServer();
    
    // 停止音频
    stopAudioInput();
    stopAudioOutput();
    
    // 停止定时器
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }
    if (m_statusUpdateTimer) {
        m_statusUpdateTimer->stop();
    }
    
    // 保存配置
    saveConfiguration();
    
    m_initialized = false;
    qDebug() << "DeskPetController shutdown complete";
}

bool DeskPetController::connectToServer()
{
    if (!m_initialized) {
        qCritical() << "DeskPetController not initialized";
        return false;
    }
    
    if (m_webSocketManager->isConnected()) {
        qWarning() << "Already connected to server";
        return true;
    }
    
    // 重新加载配置，确保配置是最新的
    loadConfiguration();
    
    qDebug() << "Connecting to server:" << m_serverUrl;
    qDebug() << "DeskPetController - Device ID:" << m_deviceId;
    qDebug() << "DeskPetController - Client ID:" << m_clientId;
    qDebug() << "DeskPetController - Access Token:" << m_accessToken;
    
    // 设置设备信息
    m_webSocketManager->setDeviceId(m_deviceId);
    m_webSocketManager->setClientId(m_clientId);
    m_webSocketManager->setAccessToken(m_accessToken);
    
    // 连接服务器
    bool success = m_webSocketManager->connectToServer(m_serverUrl, m_accessToken);
    
    if (success) {
        qDebug() << "Connection request sent successfully";
    } else {
        qCritical() << "Failed to send connection request";
    }
    
    return success;
}

void DeskPetController::disconnectFromServer()
{
    if (m_webSocketManager) {
        m_webSocketManager->disconnectFromServer();
    }
}

bool DeskPetController::isConnected() const
{
    return m_webSocketManager && m_webSocketManager->isConnected();
}

void DeskPetController::startListening()
{
    if (!isConnected()) {
        qWarning() << "Not connected to server, cannot start listening";
        return;
    }
    
    qDebug() << "Starting listening...";
    
    // 更新状态
    m_stateManager->setDeviceState(DeviceState::LISTENING);
    m_stateManager->startRecording();
    
    // 发送开始监听消息
    m_webSocketManager->sendListenStart();
    
    // 开始音频输入
    if (m_microphoneEnabled) {
        startAudioInput();
    }
}

void DeskPetController::stopListening()
{
    qDebug() << "Stopping listening...";
    
    // 更新状态
    m_stateManager->setDeviceState(DeviceState::IDLE);
    m_stateManager->stopRecording();
    
    // 发送停止监听消息
    m_webSocketManager->sendListenStop();
    
    // 停止音频输入
    stopAudioInput();
}

void DeskPetController::sendTextMessage(const QString &text)
{
    if (!isConnected()) {
        qWarning() << "Not connected to server, cannot send message";
        return;
    }
    
    qDebug() << "Sending text message:" << text;
    
    // 发送唤醒词检测消息
    m_webSocketManager->sendWakeWordDetected(text);
}

void DeskPetController::sendAudioMessage(const QByteArray &audioData)
{
    if (!isConnected()) {
        qWarning() << "Not connected to server, cannot send audio";
        return;
    }
    
    qDebug() << "Sending audio message, size:" << audioData.size();
    
    // 发送音频数据
    m_webSocketManager->sendAudioData(audioData);
}

void DeskPetController::abortSpeaking()
{
    if (!isConnected()) {
        qWarning() << "Not connected to server, cannot abort speaking";
        return;
    }
    
    qDebug() << "Aborting speaking...";
    
    // 更新状态
    m_stateManager->setDeviceState(DeviceState::IDLE);
    m_stateManager->stopPlaying();
    
    // 发送中止消息
    m_webSocketManager->sendAbortSpeaking();
    
    // 停止音频输出
    stopAudioOutput();
}

PetBehavior DeskPetController::getCurrentBehavior() const
{
    return m_stateManager ? m_stateManager->getCurrentBehavior() : PetBehavior::IDLE;
}

DeviceState DeskPetController::getCurrentDeviceState() const
{
    return m_stateManager ? m_stateManager->getCurrentDeviceState() : DeviceState::DISCONNECTED;
}

bool DeskPetController::isListening() const
{
    return m_stateManager ? m_stateManager->isListening() : false;
}

bool DeskPetController::isSpeaking() const
{
    return m_stateManager ? m_stateManager->isSpeaking() : false;
}

void DeskPetController::setServerUrl(const QString &url)
{
    m_serverUrl = url;
    qDebug() << "Server URL set to:" << url;
}

void DeskPetController::setAccessToken(const QString &token)
{
    m_accessToken = token;
    qDebug() << "Access token set";
}

void DeskPetController::setDeviceId(const QString &deviceId)
{
    m_deviceId = deviceId;
    qDebug() << "Device ID set to:" << deviceId;
}

void DeskPetController::setClientId(const QString &clientId)
{
    m_clientId = clientId;
    qDebug() << "Client ID set to:" << clientId;
}

void DeskPetController::setAudioEnabled(bool enabled)
{
    m_audioEnabled = enabled;
    qDebug() << "Audio enabled:" << enabled;
}

void DeskPetController::setMicrophoneEnabled(bool enabled)
{
    m_microphoneEnabled = enabled;
    qDebug() << "Microphone enabled:" << enabled;
}

void DeskPetController::setSpeakerEnabled(bool enabled)
{
    m_speakerEnabled = enabled;
    qDebug() << "Speaker enabled:" << enabled;
}

void DeskPetController::setAnimationEnabled(bool enabled)
{
    m_animationEnabled = enabled;
    qDebug() << "Animation enabled:" << enabled;
}

void DeskPetController::playAnimation(const QString &animationName)
{
    if (!m_animationEnabled) return;
    
    qDebug() << "Playing animation:" << animationName;
    emit animationRequested(animationName);
    
    // 更新Live2D动画
    updateLive2DAnimation(animationName);
}

void DeskPetController::stopCurrentAnimation()
{
    qDebug() << "Stopping current animation";
    emit animationRequested("stop");
}

void DeskPetController::processUserInput(const QString &input)
{
    qDebug() << "Processing user input:" << input;
    
    // 发送文本消息
    sendTextMessage(input);
}

void DeskPetController::processVoiceInput(const QByteArray &audioData)
{
    qDebug() << "Processing voice input, size:" << audioData.size();
    
    // 发送音频数据
    sendAudioMessage(audioData);
}

// 私有方法实现
void DeskPetController::initializeComponents()
{
    // 初始化组件
    m_webSocketManager = nullptr;
    m_stateManager = nullptr;
    m_configManager = nullptr;
    m_live2DManager = nullptr;
    m_audioBuffer = nullptr;
    m_networkManager = nullptr;
    m_heartbeatTimer = nullptr;
    m_statusUpdateTimer = nullptr;
}

void DeskPetController::setupConnections()
{
    if (!m_webSocketManager || !m_stateManager) return;
    
    // 先断开所有可能存在的旧连接，避免重复连接
    if (m_webSocketManager) {
        disconnect(m_webSocketManager, nullptr, this, nullptr);
    }
    if (m_stateManager) {
        disconnect(m_stateManager, nullptr, this, nullptr);
    }
    
    // WebSocket信号连接
    connect(m_webSocketManager, &WebSocketManager::connected, this, &DeskPetController::onWebSocketConnected);
    connect(m_webSocketManager, &WebSocketManager::disconnected, this, &DeskPetController::onWebSocketDisconnected);
    connect(m_webSocketManager, &WebSocketManager::connectionError, this, &DeskPetController::onWebSocketError);
    connect(m_webSocketManager, &WebSocketManager::messageReceived, this, &DeskPetController::onWebSocketMessageReceived);
    connect(m_webSocketManager, &WebSocketManager::ttsMessageReceived, this, &DeskPetController::onWebSocketTTSReceived);
    connect(m_webSocketManager, &WebSocketManager::sttMessageReceived, this, &DeskPetController::onWebSocketSTTReceived);
    connect(m_webSocketManager, &WebSocketManager::llmMessageReceived, this, &DeskPetController::onWebSocketLLMReceived);
    connect(m_webSocketManager, &WebSocketManager::iotCommandReceived, this, &DeskPetController::onWebSocketIoTReceived);
    connect(m_webSocketManager, &WebSocketManager::audioDataReceived, this, &DeskPetController::onWebSocketAudioReceived);
    
    // 状态管理信号连接
    connect(m_stateManager, &DeskPetStateManager::behaviorChanged, this, &DeskPetController::onBehaviorChanged);
    connect(m_stateManager, &DeskPetStateManager::audioStateChanged, this, &DeskPetController::onAudioStateChanged);
    connect(m_stateManager, &DeskPetStateManager::deviceStateChanged, this, &DeskPetController::onDeviceStateChanged);
    connect(m_stateManager, &DeskPetStateManager::animationRequested, this, &DeskPetController::onAnimationRequested);
    connect(m_stateManager, &DeskPetStateManager::animationStopped, this, &DeskPetController::onAnimationStopped);
    connect(m_stateManager, &DeskPetStateManager::startRecordingRequested, this, &DeskPetController::onStartRecordingRequested);
    connect(m_stateManager, &DeskPetStateManager::stopRecordingRequested, this, &DeskPetController::onStopRecordingRequested);
    connect(m_stateManager, &DeskPetStateManager::startPlayingRequested, this, &DeskPetController::onStartPlayingRequested);
    connect(m_stateManager, &DeskPetStateManager::stopPlayingRequested, this, &DeskPetController::onStopPlayingRequested);
    connect(m_stateManager, &DeskPetStateManager::messageToSend, this, &DeskPetController::onMessageToSend);
    connect(m_stateManager, &DeskPetStateManager::audioDataToSend, this, &DeskPetController::onAudioDataToSend);
    connect(m_stateManager, &DeskPetStateManager::petInteraction, this, &DeskPetController::onPetInteraction);
    connect(m_stateManager, &DeskPetStateManager::emotionChanged, this, &DeskPetController::onEmotionChanged);
}

void DeskPetController::setupAudio()
{
    // 设置音频格式
    m_audioConfig.sampleRate = 16000;
    m_audioConfig.channels = 1;
    m_audioConfig.sampleSize = 16;
    m_audioConfig.frameSize = 320; // 20ms at 16kHz
    m_audioConfig.codec = "opus";
    
    // 创建音频缓冲区
    m_audioBuffer = new QBuffer(this);
    m_audioBuffer->open(QIODevice::ReadWrite);
}

void DeskPetController::setupTimers()
{
    // 心跳定时器
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(30000); // 30秒
    connect(m_heartbeatTimer, &QTimer::timeout, this, &DeskPetController::onHeartbeatTimeout);
    
    // 状态更新定时器
    m_statusUpdateTimer = new QTimer(this);
    m_statusUpdateTimer->setInterval(1000); // 1秒
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &DeskPetController::onStatusUpdateTimeout);
}

void DeskPetController::loadConfiguration()
{
    if (!m_configManager) return;
    
    // 从配置文件加载设置
    m_serverUrl = m_configManager->getConfig("SYSTEM_OPTIONS.NETWORK.WEBSOCKET_URL", "wss://api.tenclass.net/xiaozhi/v1/").toString();
    m_accessToken = m_configManager->getConfig("SYSTEM_OPTIONS.NETWORK.WEBSOCKET_ACCESS_TOKEN", "").toString();
    m_deviceId = m_configManager->getConfig("SYSTEM_OPTIONS.DEVICE_ID", "").toString();
    m_clientId = m_configManager->getConfig("SYSTEM_OPTIONS.CLIENT_ID", "").toString();
    
    qDebug() << "Configuration loaded";
    qDebug() << "Server URL:" << m_serverUrl;
    qDebug() << "Access Token:" << m_accessToken;
    qDebug() << "Device ID:" << m_deviceId;
    qDebug() << "Client ID:" << m_clientId;
}

void DeskPetController::saveConfiguration()
{
    if (!m_configManager) return;
    
    // 保存设置到配置文件
    m_configManager->updateConfig("SYSTEM_OPTIONS.NETWORK.WEBSOCKET_URL", m_serverUrl);
    m_configManager->updateConfig("SYSTEM_OPTIONS.NETWORK.WEBSOCKET_ACCESS_TOKEN", m_accessToken);
    m_configManager->updateConfig("SYSTEM_OPTIONS.DEVICE_ID", m_deviceId);
    m_configManager->updateConfig("SYSTEM_OPTIONS.CLIENT_ID", m_clientId);
    
    m_configManager->saveConfig();
    qDebug() << "Configuration saved";
}

void DeskPetController::startAudioInput()
{
    if (!m_audioEnabled || !m_microphoneEnabled) return;
    
    qDebug() << "Audio input started (simplified)";
    // 简化实现，实际项目中需要完整的音频处理
}

void DeskPetController::stopAudioInput()
{
    qDebug() << "Audio input stopped (simplified)";
    // 简化实现
}

void DeskPetController::startAudioOutput()
{
    if (!m_audioEnabled || !m_speakerEnabled) return;
    
    qDebug() << "Audio output started (simplified)";
    // 简化实现
}

void DeskPetController::stopAudioOutput()
{
    qDebug() << "Audio output stopped (simplified)";
    // 简化实现
}

QByteArray DeskPetController::processAudioData(const QByteArray &inputData)
{
    // 简单的音频处理，实际应用中可能需要编码为Opus
    return inputData;
}

void DeskPetController::playAudioData(const QByteArray &audioData)
{
    if (!m_speakerEnabled || audioData.isEmpty()) {
        qDebug() << "Audio playback disabled or empty audio data";
        return;
    }
    
    qDebug() << "Playing audio data, size:" << audioData.size() << "bytes";
    
    // 简化实现：音频播放将在DeskPetIntegration中处理
    // 这里只记录日志，实际播放由上层处理
}

void DeskPetController::handleAnimationRequest(AnimationType type)
{
    QString animationName = getAnimationName(type);
    playAnimation(animationName);
}

QString DeskPetController::getAnimationName(AnimationType type)
{
    switch (type) {
    case AnimationType::IDLE_LOOP:
        return "idle";
    case AnimationType::LISTENING:
        return "listening";
    case AnimationType::SPEAKING:
        return "speaking";
    case AnimationType::THINKING:
        return "thinking";
    case AnimationType::EXCITED:
        return "excited";
    case AnimationType::SAD:
        return "sad";
    case AnimationType::ANGRY:
        return "angry";
    case AnimationType::SLEEPING:
        return "sleeping";
    case AnimationType::WAKE_UP:
        return "wake_up";
    case AnimationType::GREETING:
        return "greeting";
    default:
        return "idle";
    }
}

void DeskPetController::updateLive2DAnimation(const QString &animationName)
{
    if (!m_live2DManager) return;
    
    // 更新Live2D动画
    // 这里需要根据实际的Live2D API来调用
    qDebug() << "Updating Live2D animation to:" << animationName;
}

void DeskPetController::handleIncomingMessage(const WebSocketMessage &message)
{
    m_stateManager->processIncomingMessage(message);
}

void DeskPetController::handleTTSMessage(const QString &text, const QString &emotion)
{
    m_stateManager->processTTSMessage(text, emotion);
}

void DeskPetController::handleSTTMessage(const QString &text)
{
    m_stateManager->processSTTMessage(text);
}

void DeskPetController::handleLLMMessage(const QString &text, const QString &emotion)
{
    m_stateManager->processLLMMessage(text, emotion);
}

void DeskPetController::handleIoTCommand(const QJsonObject &command)
{
    m_stateManager->processIoTCommand(command);
}

void DeskPetController::syncStateWithLive2D()
{
    // 同步状态到Live2D
    updateLive2DState();
}

void DeskPetController::updateLive2DState()
{
    // 更新Live2D状态
    if (!m_live2DManager) return;
    
    // 根据当前行为更新Live2D
    PetBehavior behavior = getCurrentBehavior();
    QString animationName = getAnimationName(static_cast<AnimationType>(behavior));
    updateLive2DAnimation(animationName);
}

void DeskPetController::logDebug(const QString &message)
{
    qDebug() << "[DeskPetController]" << message;
    emit debugMessage(message);
}

void DeskPetController::logError(const QString &message)
{
    qCritical() << "[DeskPetController]" << message;
    emit debugMessage("ERROR: " + message);
}

void DeskPetController::logInfo(const QString &message)
{
    qInfo() << "[DeskPetController]" << message;
    emit debugMessage("INFO: " + message);
}

// 槽函数实现
void DeskPetController::onWebSocketConnected()
{
    qDebug() << "WebSocket connected";
    emit connected();
}

void DeskPetController::onWebSocketDisconnected()
{
    qDebug() << "WebSocket disconnected";
    emit disconnected();
}

void DeskPetController::onWebSocketError(const QString &error)
{
    qCritical() << "WebSocket error:" << error;
    emit connectionError(error);
}

void DeskPetController::onWebSocketMessageReceived(const WebSocketMessage &message)
{
    handleIncomingMessage(message);
}

void DeskPetController::onWebSocketTTSReceived(const QString &text, const QString &emotion)
{
    qDebug() << "DeskPetController::onWebSocketTTSReceived - Text:" << text << "Emotion:" << emotion;
    handleTTSMessage(text, emotion);
    
    // 只有当文本不为空且不是纯表情重置信号时才发送消息
    // 避免TTS的多个state消息导致重复显示
    if (!text.isEmpty() && emotion != "neutral") {
        qDebug() << "Emitting messageReceived:" << text;
        emit messageReceived(text);
    }
}

void DeskPetController::onWebSocketSTTReceived(const QString &text)
{
    handleSTTMessage(text);
    // 发出 STT 信号给外部
    emit sttReceived(text);
}

void DeskPetController::onWebSocketLLMReceived(const QString &text, const QString &emotion)
{
    handleLLMMessage(text, emotion);
}

void DeskPetController::onWebSocketIoTReceived(const QJsonObject &command)
{
    handleIoTCommand(command);
}

void DeskPetController::onWebSocketAudioReceived(const QByteArray &audioData)
{
    // 只发出信号，不在这里播放音频，避免重复处理
    emit audioReceived(audioData);
}

void DeskPetController::onBehaviorChanged(PetBehavior newBehavior)
{
    emit behaviorChanged(newBehavior);
    syncStateWithLive2D();
}

void DeskPetController::onAudioStateChanged(AudioState newState)
{
    Q_UNUSED(newState)
    // 处理音频状态变化
}

void DeskPetController::onDeviceStateChanged(DeviceState newState)
{
    emit deviceStateChanged(newState);
}

void DeskPetController::onAnimationRequested(AnimationType type)
{
    handleAnimationRequest(type);
}

void DeskPetController::onAnimationStopped()
{
    stopCurrentAnimation();
}

void DeskPetController::onStartRecordingRequested()
{
    startAudioInput();
}

void DeskPetController::onStopRecordingRequested()
{
    stopAudioInput();
}

void DeskPetController::onStartPlayingRequested()
{
    startAudioOutput();
}

void DeskPetController::onStopPlayingRequested()
{
    stopAudioOutput();
}

void DeskPetController::onMessageToSend(const QString &message)
{
    // 处理要发送的消息
    qDebug() << "Message to send:" << message;
}

void DeskPetController::onAudioDataToSend(const QByteArray &data)
{
    // 处理要发送的音频数据
    sendAudioMessage(data);
}

void DeskPetController::onPetInteraction(const QString &interaction)
{
    emit petInteraction(interaction);
}

void DeskPetController::onEmotionChanged(const QString &emotion)
{
    emit emotionChanged(emotion);
}

void DeskPetController::onAudioInputData()
{
    qDebug() << "Audio input data (simplified)";
    // 简化实现
}

void DeskPetController::onAudioOutputData()
{
    qDebug() << "Audio output data (simplified)";
    // 简化实现
}

void DeskPetController::onAudioInputError()
{
    qWarning() << "Audio input error (simplified)";
}

void DeskPetController::onAudioOutputError()
{
    qWarning() << "Audio output error (simplified)";
}

void DeskPetController::onHeartbeatTimeout()
{
    // 发送心跳
    if (m_webSocketManager && m_webSocketManager->isConnected()) {
        // 发送ping消息
        qDebug() << "Sending heartbeat";
    }
}

void DeskPetController::onStatusUpdateTimeout()
{
    // 更新状态
    syncStateWithLive2D();
}
