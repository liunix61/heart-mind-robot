#include "DeskPetIntegration.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QThread>

DeskPetIntegration::DeskPetIntegration(QObject *parent)
    : QObject(parent)
    , m_controller(nullptr)
    , m_mainWindow(nullptr)
    , m_live2DManager(nullptr)
    , m_audioPlayer(nullptr)
    , m_statusUpdateTimer(nullptr)
    , m_heartbeatTimer(nullptr)
    , m_initialized(false)
    , m_connected(false)
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
    
    // 减少日志输出，避免影响性能
    // qDebug() << "Playing audio:" << audioData.size() << "bytes";
    
    // 使用AudioPlayer播放接收到的音频数据
    if (m_audioPlayer) {
        m_audioPlayer->playReceivedAudioData(audioData);
    } else {
        qWarning() << "AudioPlayer not initialized";
    }
}

void DeskPetIntegration::setupConnections()
{
    if (!m_controller) return;
    
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
    
    // 这里可以根据情绪更新Live2D的表情参数
    // 例如：m_live2DManager->setExpression(emotion);
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
    emit messageReceived(message);
}

void DeskPetIntegration::onControllerAudioReceived(const QByteArray &audioData)
{
    qDebug() << "Audio received, size:" << audioData.size();
    
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
