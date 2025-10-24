#include "DeskPetIntegration.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QThread>
#include <QDataStream>

DeskPetIntegration::DeskPetIntegration(QObject *parent)
    : QObject(parent)
    , m_controller(nullptr)
    , m_mainWindow(nullptr)
    , m_live2DManager(nullptr)
    , m_audioPlayer(nullptr)
    , m_statusUpdateTimer(nullptr)
    , m_heartbeatTimer(nullptr)
    , m_lipSyncTimer(nullptr)
    , m_initialized(false)
    , m_connected(false)
    , m_lipSyncEnabled(true)  // 默认启用口型同步
{
    // 初始化配置
    m_serverUrl = "wss://api.tenclass.net/xiaozhi/v1/";
    m_accessToken = "";
    m_deviceId = "";
    m_clientId = "";
}

DeskPetIntegration::~DeskPetIntegration()
{
    shutdown();
}

bool DeskPetIntegration::initialize(MainWindow *mainWindow)
{
    if (m_initialized) {
        qWarning() << "DeskPetIntegration already initialized";
        return true;
    }
    
    if (!mainWindow) {
        qCritical() << "MainWindow is null";
        return false;
    }
    
    qDebug() << "Initializing DeskPetIntegration...";
    
    try {
        // 保存主窗口引用
        m_mainWindow = mainWindow;
        
        // 获取Live2D管理器
        m_live2DManager = LAppLive2DManager::GetInstance();
        if (!m_live2DManager) {
            qCritical() << "Failed to get Live2D manager";
            return false;
        }
        
        // 创建桌宠控制器
        m_controller = new DeskPetController(this);
        if (!m_controller->initialize()) {
            qCritical() << "Failed to initialize DeskPetController";
            return false;
        }
        
        // 创建音频播放器
        m_audioPlayer = new AudioPlayer();
        if (!m_audioPlayer) {
            qCritical() << "Failed to create AudioPlayer";
            return false;
        }
        
        // 设置连接
        setupConnections();
        
        // 设置定时器
        setupTimers();
        
        // 加载配置
        loadConfiguration();
        
        m_initialized = true;
        qDebug() << "DeskPetIntegration initialized successfully";
        return true;
        
    } catch (const std::exception &e) {
        qCritical() << "Failed to initialize DeskPetIntegration:" << e.what();
        return false;
    }
}

void DeskPetIntegration::shutdown()
{
    if (!m_initialized) return;
    
    qDebug() << "Shutting down DeskPetIntegration...";
    
    // 断开连接
    disconnectFromServer();
    
    // 停止定时器
    if (m_statusUpdateTimer) {
        m_statusUpdateTimer->stop();
        m_statusUpdateTimer->deleteLater();
        m_statusUpdateTimer = nullptr;
    }
    
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        m_heartbeatTimer->deleteLater();
        m_heartbeatTimer = nullptr;
    }
    
    // 关闭控制器
    if (m_controller) {
        m_controller->shutdown();
        m_controller->deleteLater();
        m_controller = nullptr;
    }
    
    // 清理音频播放器
    if (m_audioPlayer) {
        delete m_audioPlayer;
        m_audioPlayer = nullptr;
    }
    
    // 保存配置
    saveConfiguration();
    
    m_initialized = false;
    m_connected = false;
    qDebug() << "DeskPetIntegration shutdown complete";
}

bool DeskPetIntegration::connectToServer()
{
    if (!m_initialized) {
        qCritical() << "DeskPetIntegration not initialized";
        return false;
    }
    
    if (m_connected) {
        qWarning() << "Already connected to server";
        return true;
    }
    
    qDebug() << "Connecting to server...";
    
    // 设置控制器配置
    m_controller->setServerUrl(m_serverUrl);
    m_controller->setAccessToken(m_accessToken);
    m_controller->setDeviceId(m_deviceId);
    m_controller->setClientId(m_clientId);
    
    // 连接服务器
    bool success = m_controller->connectToServer();
    
    if (success) {
        qDebug() << "Connection request sent successfully";
    } else {
        qCritical() << "Failed to send connection request";
    }
    
    return success;
}

void DeskPetIntegration::disconnectFromServer()
{
    if (m_controller) {
        m_controller->disconnectFromServer();
    }
    m_connected = false;
}

bool DeskPetIntegration::isConnected() const
{
    return m_connected && m_controller && m_controller->isConnected();
}

void DeskPetIntegration::startListening()
{
    if (!isConnected()) {
        qWarning() << "Not connected to server, cannot start listening";
        return;
    }
    
    qDebug() << "Starting listening...";
    m_controller->startListening();
}

void DeskPetIntegration::stopListening()
{
    qDebug() << "Stopping listening...";
    m_controller->stopListening();
}

void DeskPetIntegration::sendTextMessage(const QString &text)
{
    if (!isConnected()) {
        qWarning() << "Not connected to server, cannot send message";
        return;
    }
    
    // 检查是否正在说话，如果是则中断
    if (isSpeaking() || getCurrentDeviceState() == DeviceState::SPEAKING) {
        qDebug() << "DeskPetIntegration: User interruption detected, clearing audio queue";
        
        // 清空音频队列
        if (m_audioPlayer) {
            m_audioPlayer->clearAudioQueue();
        }
        
        // 发送中断消息到服务器
        m_controller->abortSpeaking();
    }
    
    qDebug() << "Sending text message:" << text;
    m_controller->sendTextMessage(text);
}

void DeskPetIntegration::sendVoiceMessage(const QByteArray &audioData)
{
    if (!isConnected()) {
        qWarning() << "Not connected to server, cannot send audio";
        return;
    }
    
    qDebug() << "Sending voice message, size:" << audioData.size();
    m_controller->sendAudioMessage(audioData);
}

void DeskPetIntegration::sendAudioData(const QByteArray &audioData)
{
    if (!isConnected()) {
        qWarning() << "Not connected to server, cannot send audio data";
        return;
    }
    
    // 直接发送音频流数据（已编码的Opus数据）
    m_controller->sendAudioMessage(audioData);
}

void DeskPetIntegration::abortSpeaking()
{
    qDebug() << "Aborting speaking...";
    m_controller->abortSpeaking();
}

PetBehavior DeskPetIntegration::getCurrentBehavior() const
{
    return m_controller ? m_controller->getCurrentBehavior() : PetBehavior::IDLE;
}

DeviceState DeskPetIntegration::getCurrentDeviceState() const
{
    return m_controller ? m_controller->getCurrentDeviceState() : DeviceState::DISCONNECTED;
}

bool DeskPetIntegration::isListening() const
{
    return m_controller ? m_controller->isListening() : false;
}

bool DeskPetIntegration::isSpeaking() const
{
    return m_controller ? m_controller->isSpeaking() : false;
}

void DeskPetIntegration::loadConfiguration()
{
    // 从配置文件加载设置
    // 这里可以使用ConfigManager来加载配置
    qDebug() << "Loading configuration...";
    
    // 设置默认值
    if (m_serverUrl.isEmpty()) {
        m_serverUrl = "wss://api.tenclass.net/xiaozhi/v1/";
    }
    
    qDebug() << "Configuration loaded";
}

void DeskPetIntegration::saveConfiguration()
{
    // 保存设置到配置文件
    qDebug() << "Saving configuration...";
    
    // 这里可以使用ConfigManager来保存配置
    qDebug() << "Configuration saved";
}

void DeskPetIntegration::setServerUrl(const QString &url)
{
    m_serverUrl = url;
    qDebug() << "Server URL set to:" << url;
}

void DeskPetIntegration::setAccessToken(const QString &token)
{
    m_accessToken = token;
    qDebug() << "Access token set";
}

void DeskPetIntegration::setAudioEnabled(bool enabled)
{
    if (m_controller) {
        m_controller->setAudioEnabled(enabled);
    }
    qDebug() << "Audio enabled:" << enabled;
}

void DeskPetIntegration::setMicrophoneEnabled(bool enabled)
{
    if (m_controller) {
        m_controller->setMicrophoneEnabled(enabled);
    }
    qDebug() << "Microphone enabled:" << enabled;
}

void DeskPetIntegration::setSpeakerEnabled(bool enabled)
{
    if (m_controller) {
        m_controller->setSpeakerEnabled(enabled);
    }
    qDebug() << "Speaker enabled:" << enabled;
}

void DeskPetIntegration::setAnimationEnabled(bool enabled)
{
    if (m_controller) {
        m_controller->setAnimationEnabled(enabled);
    }
    qDebug() << "Animation enabled:" << enabled;
}

void DeskPetIntegration::playAnimation(const QString &animationName)
{
    if (m_controller) {
        m_controller->playAnimation(animationName);
    }
    qDebug() << "Playing animation:" << animationName;
}

void DeskPetIntegration::stopCurrentAnimation()
{
    if (m_controller) {
        m_controller->stopCurrentAnimation();
    }
    qDebug() << "Stopping current animation";
}

void DeskPetIntegration::processUserInput(const QString &input)
{
    qDebug() << "Processing user input:" << input;
    m_controller->processUserInput(input);
}

void DeskPetIntegration::processVoiceInput(const QByteArray &audioData)
{
    qDebug() << "Processing voice input, size:" << audioData.size();
    m_controller->processVoiceInput(audioData);
}

void DeskPetIntegration::playAudioData(const QByteArray &audioData)
{
    if (audioData.isEmpty()) {
        return;
    }
    
    qDebug() << "=== playAudioData called:" << audioData.size() << "bytes (Opus encoded)";
    
    // 使用AudioPlayer播放接收到的Opus编码音频数据
    // AudioPlayer会解码并播放，同时通过信号发射解码后的PCM数据用于口型同步
    if (m_audioPlayer) {
        m_audioPlayer->playReceivedAudioData(audioData);
    } else {
        qWarning() << "AudioPlayer not initialized!";
    }
}

void DeskPetIntegration::setupConnections()
{
    if (!m_controller) return;
    
    // 连接音频播放器信号 - 用于口型同步
    if (m_audioPlayer) {
        connect(m_audioPlayer, &AudioPlayer::audioDecoded,
                this, &DeskPetIntegration::onAudioDecoded);
        qDebug() << "Audio player signal connected for lip sync";
    }
    
    // 连接控制器信号
    connect(m_controller, &DeskPetController::connected, this, &DeskPetIntegration::onControllerConnected);
    connect(m_controller, &DeskPetController::disconnected, this, &DeskPetIntegration::onControllerDisconnected);
    connect(m_controller, &DeskPetController::connectionError, this, &DeskPetIntegration::onControllerError);
    connect(m_controller, &DeskPetController::behaviorChanged, this, &DeskPetIntegration::onControllerBehaviorChanged);
    connect(m_controller, &DeskPetController::deviceStateChanged, this, &DeskPetIntegration::onControllerDeviceStateChanged);
    connect(m_controller, &DeskPetController::messageReceived, this, &DeskPetIntegration::onControllerMessageReceived);
    connect(m_controller, &DeskPetController::audioReceived, this, &DeskPetIntegration::onControllerAudioReceived);
    connect(m_controller, &DeskPetController::emotionChanged, this, &DeskPetIntegration::onControllerEmotionChanged);
    connect(m_controller, &DeskPetController::petInteraction, this, &DeskPetIntegration::onControllerPetInteraction);
    connect(m_controller, &DeskPetController::animationRequested, this, &DeskPetIntegration::onControllerAnimationRequested);
    connect(m_controller, &DeskPetController::debugMessage, this, &DeskPetIntegration::onControllerDebugMessage);
    
    // 连接 STT 信号用于退出音乐模式
    connect(m_controller, &DeskPetController::sttReceived, this, &DeskPetIntegration::onControllerSTTReceived);
}

void DeskPetIntegration::setupTimers()
{
    // 状态更新定时器
    m_statusUpdateTimer = new QTimer(this);
    m_statusUpdateTimer->setInterval(1000); // 1秒
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &DeskPetIntegration::onStatusUpdateTimeout);
    m_statusUpdateTimer->start();
    
    // 心跳定时器
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(30000); // 30秒
    connect(m_heartbeatTimer, &QTimer::timeout, this, &DeskPetIntegration::onHeartbeatTimeout);
    m_heartbeatTimer->start();
}

void DeskPetIntegration::updateLive2DState()
{
    if (!m_live2DManager) return;
    
    // 根据当前行为更新Live2D状态
    PetBehavior behavior = getCurrentBehavior();
    handleBehaviorChange(behavior);
}

void DeskPetIntegration::handleBehaviorChange(PetBehavior behavior)
{
    if (!m_live2DManager) return;
    
    // 根据行为更新Live2D动画
    switch (behavior) {
    case PetBehavior::IDLE:
        // 播放空闲动画
        break;
    case PetBehavior::LISTENING:
        // 播放监听动画
        break;
    case PetBehavior::SPEAKING:
        // 播放说话动画
        break;
    case PetBehavior::THINKING:
        // 播放思考动画
        break;
    case PetBehavior::EXCITED:
        // 播放兴奋动画
        break;
    case PetBehavior::SAD:
        // 播放悲伤动画
        break;
    case PetBehavior::ANGRY:
        // 播放愤怒动画
        break;
    case PetBehavior::SLEEPING:
        // 播放睡眠动画
        break;
    default:
        break;
    }
}

void DeskPetIntegration::handleEmotionChange(const QString &emotion)
{
    if (!m_live2DManager) return;
    
    // 根据情绪更新Live2D表情
    qDebug() << "Handling emotion change:" << emotion;
    
    // 情绪到 Live2D 表情的映射
    // Haru 模型有 F01-F08 表情
    QString expressionName;
    
    // 特殊处理：neutral 用于重置表情
    if (emotion.isEmpty() || emotion == "neutral") {
        qDebug() << "Resetting expression to neutral (F01)";
        if (m_live2DManager->GetModel(0)) {
            m_live2DManager->GetModel(0)->SetExpression("F01");  // F01 是最温和的微笑
        }
        return;
    }
    
    // 🎭 表情映射 - 全部使用最夸张的表情（按参数数量从多到少）：
    // F07 (12参数) = 害羞/脸红 - 眉毛复杂变化 + 脸红效果
    // F04 (11参数) = 惊讶 - 眼睛睁大 + 眉毛大幅变化
    // F03 (10参数) = 生气 - 眉毛皱起 + 嘴巴大变化
    // F06 (6参数)  = 兴奋 - 眼睛放大2倍！最夸张
    // F02 (6参数)  = 悲伤 - 眉毛下垂 + 嘴巴张开
    // F08 (5参数)  = 疲惫 - 眼睛变小 + 嘴巴大变化
    // F05 (4参数)  = 开心 - 眯眼笑（眼睛完全闭上）
    // F01 (1参数)  = 微笑 - 仅嘴巴微调（不明显，避免使用）
    
    if (emotion.contains("happy", Qt::CaseInsensitive) || 
        emotion.contains("joy", Qt::CaseInsensitive) ||
        emotion.contains("开心", Qt::CaseInsensitive) ||
        emotion.contains("高兴", Qt::CaseInsensitive) ||
        emotion.contains("cool", Qt::CaseInsensitive)) {
        expressionName = "F05";  // 开心 - 眯眼笑（眼睛完全闭上，非常明显）
    }
    else if (emotion.contains("excited", Qt::CaseInsensitive) || 
             emotion.contains("兴奋", Qt::CaseInsensitive) ||
             emotion.contains("激动", Qt::CaseInsensitive)) {
        expressionName = "F06";  // 兴奋 - 眼睛放大2倍（最夸张！）
    }
    else if (emotion.contains("surprised", Qt::CaseInsensitive) || 
             emotion.contains("shock", Qt::CaseInsensitive) ||
             emotion.contains("惊讶", Qt::CaseInsensitive) ||
             emotion.contains("吃惊", Qt::CaseInsensitive)) {
        expressionName = "F04";  // 惊讶 - 11个参数，非常明显
    }
    else if (emotion.contains("angry", Qt::CaseInsensitive) || 
             emotion.contains("mad", Qt::CaseInsensitive) ||
             emotion.contains("生气", Qt::CaseInsensitive) ||
             emotion.contains("愤怒", Qt::CaseInsensitive)) {
        expressionName = "F03";  // 生气 - 10个参数，眉毛+嘴巴大变化
    }
    else if (emotion.contains("shy", Qt::CaseInsensitive) || 
             emotion.contains("embarrassed", Qt::CaseInsensitive) ||
             emotion.contains("害羞", Qt::CaseInsensitive) ||
             emotion.contains("羞涩", Qt::CaseInsensitive)) {
        expressionName = "F07";  // 害羞 - 12个参数+脸红效果（最复杂！）
    }
    else if (emotion.contains("sad", Qt::CaseInsensitive) || 
             emotion.contains("upset", Qt::CaseInsensitive) ||
             emotion.contains("悲伤", Qt::CaseInsensitive) ||
             emotion.contains("难过", Qt::CaseInsensitive)) {
        expressionName = "F02";  // 悲伤 - 6个参数，明显
    }
    else if (emotion.contains("tired", Qt::CaseInsensitive) || 
             emotion.contains("sleepy", Qt::CaseInsensitive) ||
             emotion.contains("累", Qt::CaseInsensitive) ||
             emotion.contains("疲惫", Qt::CaseInsensitive)) {
        expressionName = "F08";  // 疲惫 - 5个参数，眼睛变小
    }
    else if (emotion.contains("thinking", Qt::CaseInsensitive) || 
             emotion.contains("confused", Qt::CaseInsensitive) ||
             emotion.contains("思考", Qt::CaseInsensitive) ||
             emotion.contains("疑惑", Qt::CaseInsensitive)) {
        expressionName = "F04";  // 思考 - 用惊讶表情
    }
    else {
        // 默认使用眯眼笑（比F01更明显）
        expressionName = "F05";  // 默认开心表情
    }
    
    // 调用 Live2D 管理器设置表情
    if (m_live2DManager->GetModel(0)) {
        qDebug() << "Setting Live2D expression:" << expressionName << "for emotion:" << emotion;
        m_live2DManager->GetModel(0)->SetExpression(expressionName.toUtf8().constData());
    } else {
        qDebug() << "Live2D model not available for expression update";
    }
}

void DeskPetIntegration::handleAnimationRequest(const QString &animationName)
{
    if (!m_live2DManager) return;
    
    // 播放指定的动画
    qDebug() << "Playing animation:" << animationName;
    
    // 这里可以调用Live2D的动画播放方法
    // 例如：m_live2DManager->playAnimation(animationName);
}

void DeskPetIntegration::logDebug(const QString &message)
{
    qDebug() << "[DeskPetIntegration]" << message;
    emit debugMessage(message);
}

void DeskPetIntegration::logError(const QString &message)
{
    qCritical() << "[DeskPetIntegration]" << message;
    emit debugMessage("ERROR: " + message);
}

void DeskPetIntegration::logInfo(const QString &message)
{
    qInfo() << "[DeskPetIntegration]" << message;
    emit debugMessage("INFO: " + message);
}

// 槽函数实现
void DeskPetIntegration::onControllerConnected()
{
    qDebug() << "Controller connected";
    m_connected = true;
    emit connected();
}

void DeskPetIntegration::onControllerDisconnected()
{
    qDebug() << "Controller disconnected";
    m_connected = false;
    emit disconnected();
}

void DeskPetIntegration::onControllerError(const QString &error)
{
    qCritical() << "Controller error:" << error;
    emit connectionError(error);
}

void DeskPetIntegration::onControllerBehaviorChanged(PetBehavior newBehavior)
{
    qDebug() << "Behavior changed to:" << static_cast<int>(newBehavior);
    emit behaviorChanged(newBehavior);
    handleBehaviorChange(newBehavior);
}

void DeskPetIntegration::onControllerDeviceStateChanged(DeviceState newState)
{
    qDebug() << "Device state changed to:" << static_cast<int>(newState);
    emit deviceStateChanged(newState);
}

void DeskPetIntegration::onControllerMessageReceived(const QString &message)
{
    qDebug() << "Message received:" << message;
    
    // 检查是否是音乐相关的消息 - 禁用口型同步
    if (message.startsWith("% play_music", Qt::CaseInsensitive) || 
        message.startsWith("% search_music", Qt::CaseInsensitive)) {
        qDebug() << "*** Music playback detected - disabling lip sync ***";
        m_lipSyncEnabled = false;
    } 
    // 注意：不再通过普通消息自动启用口型同步
    // 只有在收到 STT（用户说话）时才会重新启用
    
    emit messageReceived(message);
}

void DeskPetIntegration::onControllerSTTReceived(const QString &text)
{
    // STT 表示用户在说话，退出音乐模式，重新启用口型同步
    if (!m_lipSyncEnabled) {
        qDebug() << "*** User speech detected (STT) - enabling lip sync ***";
        m_lipSyncEnabled = true;
    }
}

void DeskPetIntegration::onControllerAudioReceived(const QByteArray &audioData)
{
    qDebug() << "========================================";
    qDebug() << "=== Audio received from WebSocket!";
    qDebug() << "=== Size:" << audioData.size() << "bytes";
    qDebug() << "========================================";
    
    // 播放接收到的音频数据
    playAudioData(audioData);
    
    // 发出信号供其他组件使用
    emit audioReceived(audioData);
}

void DeskPetIntegration::onControllerEmotionChanged(const QString &emotion)
{
    qDebug() << "Emotion changed to:" << emotion;
    emit emotionChanged(emotion);
    handleEmotionChange(emotion);
}

void DeskPetIntegration::onControllerPetInteraction(const QString &interaction)
{
    qDebug() << "Pet interaction:" << interaction;
    emit petInteraction(interaction);
}

void DeskPetIntegration::onControllerAnimationRequested(const QString &animationName)
{
    qDebug() << "Animation requested:" << animationName;
    emit animationRequested(animationName);
    handleAnimationRequest(animationName);
}

void DeskPetIntegration::onControllerDebugMessage(const QString &message)
{
    qDebug() << "Controller debug:" << message;
    emit debugMessage(message);
}

void DeskPetIntegration::onStatusUpdateTimeout()
{
    // 更新状态
    updateLive2DState();
}

void DeskPetIntegration::onHeartbeatTimeout()
{
    // 发送心跳
    if (isConnected()) {
        qDebug() << "Sending heartbeat";
        // 这里可以发送心跳消息
    }
}

void DeskPetIntegration::onAudioDecoded(const QByteArray &pcmData)
{
    if (pcmData.isEmpty() || !m_live2DManager) {
        return;
    }
    
    // 只在启用口型同步时才更新（播放音乐时禁用）
    if (m_lipSyncEnabled) {
        m_live2DManager->UpdateLipSyncFromPCM(pcmData, 24000);
        qDebug() << "✓ Lip sync updated from" << pcmData.size() << "bytes PCM";
    } else {
        qDebug() << "○ Lip sync disabled (music playback)";
    }
}

QByteArray DeskPetIntegration::convertPCMToWAV(const QByteArray &pcmData, int sampleRate, int channels, int bitsPerSample)
{
    if (pcmData.isEmpty()) {
        return QByteArray();
    }
    
    QByteArray wavData;
    QDataStream stream(&wavData, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    int bytesPerSample = bitsPerSample / 8;
    int byteRate = sampleRate * channels * bytesPerSample;
    int blockAlign = channels * bytesPerSample;
    int dataSize = pcmData.size();
    int fileSize = 36 + dataSize;
    
    // RIFF header
    stream.writeRawData("RIFF", 4);
    stream << (quint32)fileSize;
    stream.writeRawData("WAVE", 4);
    
    // fmt chunk
    stream.writeRawData("fmt ", 4);
    stream << (quint32)16;              // fmt chunk size
    stream << (quint16)1;               // audio format (1 = PCM)
    stream << (quint16)channels;        // number of channels
    stream << (quint32)sampleRate;      // sample rate
    stream << (quint32)byteRate;        // byte rate
    stream << (quint16)blockAlign;      // block align
    stream << (quint16)bitsPerSample;   // bits per sample
    
    // data chunk
    stream.writeRawData("data", 4);
    stream << (quint32)dataSize;
    stream.writeRawData(pcmData.constData(), dataSize);
    
    return wavData;
}
