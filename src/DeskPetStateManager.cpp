#include "DeskPetStateManager.h"
#include <QDebug>
#include <QMutexLocker>
#include <QRegularExpression>

DeskPetStateManager::DeskPetStateManager(QObject *parent)
    : QObject(parent)
    , m_currentBehavior(PetBehavior::IDLE)
    , m_currentAudioState(AudioState::SILENT)
    , m_currentDeviceState(DeviceState::DISCONNECTED)
    , m_currentAnimation(AnimationType::IDLE_LOOP)
    , m_behaviorTimer(nullptr)
    , m_audioTimer(nullptr)
    , m_processingTimer(nullptr)
    , m_messageQueueTimer(nullptr)
    , m_autoResponse(true)
    , m_voiceEnabled(true)
    , m_animationEnabled(true)
    , m_idleTimeout(30000)      // 30秒
    , m_listeningTimeout(10000)  // 10秒
    , m_speakingTimeout(30000)  // 30秒
    , m_processingTimeout(5000)  // 5秒
{
    initializeTimers();
}

DeskPetStateManager::~DeskPetStateManager()
{
    if (m_behaviorTimer) m_behaviorTimer->deleteLater();
    if (m_audioTimer) m_audioTimer->deleteLater();
    if (m_processingTimer) m_processingTimer->deleteLater();
    if (m_messageQueueTimer) m_messageQueueTimer->deleteLater();
}

void DeskPetStateManager::initializeTimers()
{
    // 行为定时器
    m_behaviorTimer = new QTimer(this);
    m_behaviorTimer->setSingleShot(true);
    connect(m_behaviorTimer, &QTimer::timeout, this, &DeskPetStateManager::onBehaviorTimeout);
    
    // 音频定时器
    m_audioTimer = new QTimer(this);
    m_audioTimer->setSingleShot(true);
    connect(m_audioTimer, &QTimer::timeout, this, &DeskPetStateManager::onAudioTimeout);
    
    // 处理定时器
    m_processingTimer = new QTimer(this);
    m_processingTimer->setSingleShot(true);
    connect(m_processingTimer, &QTimer::timeout, this, &DeskPetStateManager::onProcessingTimeout);
    
    // 消息队列定时器
    m_messageQueueTimer = new QTimer(this);
    m_messageQueueTimer->setInterval(100); // 100ms间隔处理消息
    connect(m_messageQueueTimer, &QTimer::timeout, this, &DeskPetStateManager::processMessageQueue);
    m_messageQueueTimer->start();
}

PetBehavior DeskPetStateManager::getCurrentBehavior() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_currentBehavior;
}

AudioState DeskPetStateManager::getCurrentAudioState() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_currentAudioState;
}

DeviceState DeskPetStateManager::getCurrentDeviceState() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_currentDeviceState;
}

void DeskPetStateManager::setBehavior(PetBehavior behavior)
{
    {
        QMutexLocker locker(&m_queueMutex);
        if (m_currentBehavior == behavior) return;
        m_currentBehavior = behavior;
    }
    
    qDebug() << "Pet behavior changed to:" << static_cast<int>(behavior);
    emit behaviorChanged(behavior);
    
    // 更新动画
    updateAnimationBasedOnBehavior();
    
    // 设置定时器
    switch (behavior) {
    case PetBehavior::IDLE:
        m_behaviorTimer->start(m_idleTimeout);
        break;
    case PetBehavior::LISTENING:
        m_behaviorTimer->start(m_listeningTimeout);
        break;
    case PetBehavior::SPEAKING:
        m_behaviorTimer->start(m_speakingTimeout);
        break;
    case PetBehavior::THINKING:
        m_behaviorTimer->start(m_processingTimeout);
        break;
    default:
        m_behaviorTimer->stop();
        break;
    }
}

void DeskPetStateManager::setAudioState(AudioState state)
{
    {
        QMutexLocker locker(&m_queueMutex);
        if (m_currentAudioState == state) return;
        m_currentAudioState = state;
    }
    
    qDebug() << "Audio state changed to:" << static_cast<int>(state);
    emit audioStateChanged(state);
    
    // 设置音频定时器
    switch (state) {
    case AudioState::RECORDING:
        m_audioTimer->start(m_listeningTimeout);
        break;
    case AudioState::PLAYING:
        m_audioTimer->start(m_speakingTimeout);
        break;
    case AudioState::PROCESSING:
        m_audioTimer->start(m_processingTimeout);
        break;
    default:
        m_audioTimer->stop();
        break;
    }
}

void DeskPetStateManager::setDeviceState(DeviceState state)
{
    {
        QMutexLocker locker(&m_queueMutex);
        if (m_currentDeviceState == state) return;
        m_currentDeviceState = state;
    }
    
    qDebug() << "Device state changed to:" << static_cast<int>(state);
    emit deviceStateChanged(state);
    
    // 根据设备状态更新行为
    updateBehaviorBasedOnDeviceState();
}

void DeskPetStateManager::playAnimation(AnimationType type)
{
    if (m_currentAnimation == type) return;
    
    m_currentAnimation = type;
    qDebug() << "Playing animation:" << static_cast<int>(type);
    emit animationRequested(type);
}

void DeskPetStateManager::stopCurrentAnimation()
{
    qDebug() << "Stopping current animation";
    emit animationStopped();
}

AnimationType DeskPetStateManager::getCurrentAnimation() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_currentAnimation;
}

void DeskPetStateManager::processIncomingMessage(const WebSocketMessage &message)
{
    qDebug() << "Processing incoming message, type:" << static_cast<int>(message.type);
    
    switch (message.type) {
    case MessageType::TTS:
        processTTSMessage(message.data["text"].toString(), message.data["emotion"].toString());
        break;
    case MessageType::STT:
        processSTTMessage(message.data["text"].toString());
        break;
    case MessageType::LLM:
        processLLMMessage(message.data["text"].toString(), message.data["emotion"].toString());
        break;
    case MessageType::IOT:
        processIoTCommand(message.data["command"].toObject());
        break;
    default:
        qDebug() << "Unknown message type:" << static_cast<int>(message.type);
        break;
    }
}

void DeskPetStateManager::processTTSMessage(const QString &text, const QString &emotion)
{
    qDebug() << "Processing TTS message:" << text << "emotion:" << emotion;
    
    setBehavior(PetBehavior::SPEAKING);
    setAudioState(AudioState::PLAYING);
    
    // 处理情绪
    handleEmotionChange(emotion);
    
    // 发送消息到队列
    if (!text.isEmpty()) {
        queueMessage(text);
    }
}

void DeskPetStateManager::processSTTMessage(const QString &text)
{
    qDebug() << "Processing STT message:" << text;
    
    setBehavior(PetBehavior::LISTENING);
    setAudioState(AudioState::RECORDING);
    
    // 处理文本中的情绪
    processTextForEmotion(text);
}

void DeskPetStateManager::processLLMMessage(const QString &text, const QString &emotion)
{
    qDebug() << "Processing LLM message:" << text << "emotion:" << emotion;
    
    setBehavior(PetBehavior::THINKING);
    setAudioState(AudioState::PROCESSING);
    
    // 处理情绪
    handleEmotionChange(emotion);
    
    // 处理文本情绪
    processTextForEmotion(text);
}

void DeskPetStateManager::processIoTCommand(const QJsonObject &command)
{
    qDebug() << "Processing IoT command:" << command;
    
    QString commandType = command["type"].toString();
    QString action = command["action"].toString();
    
    if (commandType == "pet_interaction") {
        emit petInteraction(action);
        
        if (action == "feed") {
            setBehavior(PetBehavior::EXCITED);
        } else if (action == "play") {
            setBehavior(PetBehavior::EXCITED);
        } else if (action == "sleep") {
            setBehavior(PetBehavior::SLEEPING);
        }
    }
}

void DeskPetStateManager::startRecording()
{
    setAudioState(AudioState::RECORDING);
    emit startRecordingRequested();
}

void DeskPetStateManager::stopRecording()
{
    setAudioState(AudioState::SILENT);
    emit stopRecordingRequested();
}

void DeskPetStateManager::startPlaying()
{
    setAudioState(AudioState::PLAYING);
    emit startPlayingRequested();
}

void DeskPetStateManager::stopPlaying()
{
    setAudioState(AudioState::SILENT);
    emit stopPlayingRequested();
}

bool DeskPetStateManager::isIdle() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_currentBehavior == PetBehavior::IDLE;
}

bool DeskPetStateManager::isListening() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_currentBehavior == PetBehavior::LISTENING;
}

bool DeskPetStateManager::isSpeaking() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_currentBehavior == PetBehavior::SPEAKING;
}

bool DeskPetStateManager::isProcessing() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_currentBehavior == PetBehavior::THINKING;
}

void DeskPetStateManager::setAutoResponse(bool enabled)
{
    m_autoResponse = enabled;
}

void DeskPetStateManager::setVoiceEnabled(bool enabled)
{
    m_voiceEnabled = enabled;
}

void DeskPetStateManager::setAnimationEnabled(bool enabled)
{
    m_animationEnabled = enabled;
}

void DeskPetStateManager::queueMessage(const QString &message)
{
    QMutexLocker locker(&m_queueMutex);
    m_messageQueue.enqueue(message);
    qDebug() << "Message queued, queue size:" << m_messageQueue.size();
}

QString DeskPetStateManager::dequeueMessage()
{
    QMutexLocker locker(&m_queueMutex);
    if (m_messageQueue.isEmpty()) {
        return QString();
    }
    return m_messageQueue.dequeue();
}

bool DeskPetStateManager::hasQueuedMessages() const
{
    QMutexLocker locker(&m_queueMutex);
    return !m_messageQueue.isEmpty();
}

int DeskPetStateManager::getQueueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_messageQueue.size();
}

void DeskPetStateManager::onBehaviorTimeout()
{
    qDebug() << "Behavior timeout, transitioning to idle";
    transitionToIdle();
}

void DeskPetStateManager::onAudioTimeout()
{
    qDebug() << "Audio timeout, stopping audio";
    setAudioState(AudioState::SILENT);
}

void DeskPetStateManager::onProcessingTimeout()
{
    qDebug() << "Processing timeout, transitioning to idle";
    transitionToIdle();
}

void DeskPetStateManager::processMessageQueue()
{
    if (hasQueuedMessages()) {
        QString message = dequeueMessage();
        if (!message.isEmpty()) {
            emit messageToSend(message);
        }
    }
}

void DeskPetStateManager::updateBehaviorBasedOnDeviceState()
{
    switch (m_currentDeviceState) {
    case DeviceState::IDLE:
        setBehavior(PetBehavior::IDLE);
        break;
    case DeviceState::LISTENING:
        setBehavior(PetBehavior::LISTENING);
        break;
    case DeviceState::SPEAKING:
        setBehavior(PetBehavior::SPEAKING);
        break;
    case DeviceState::CONNECTING:
        setBehavior(PetBehavior::THINKING);
        break;
    case DeviceState::DISCONNECTED:
        setBehavior(PetBehavior::SLEEPING);
        break;
    }
}

void DeskPetStateManager::updateAnimationBasedOnBehavior()
{
    if (!m_animationEnabled) return;
    
    AnimationType animation = getAnimationForBehavior(m_currentBehavior);
    playAnimation(animation);
}

void DeskPetStateManager::handleEmotionChange(const QString &emotion)
{
    if (emotion.isEmpty()) return;
    
    qDebug() << "Emotion changed to:" << emotion;
    emit emotionChanged(emotion);
    
    // 根据情绪更新行为
    if (emotion == "happy" || emotion == "excited") {
        setBehavior(PetBehavior::EXCITED);
    } else if (emotion == "sad" || emotion == "depressed") {
        setBehavior(PetBehavior::SAD);
    } else if (emotion == "angry" || emotion == "frustrated") {
        setBehavior(PetBehavior::ANGRY);
    } else if (emotion == "sleepy" || emotion == "tired") {
        setBehavior(PetBehavior::SLEEPING);
    }
}

void DeskPetStateManager::processTextForEmotion(const QString &text)
{
    QString emotion = extractEmotionFromText(text);
    if (!emotion.isEmpty()) {
        handleEmotionChange(emotion);
    }
}

QString DeskPetStateManager::extractEmotionFromText(const QString &text)
{
    // 简单的情绪关键词检测
    QString lowerText = text.toLower();
    
    if (lowerText.contains("开心") || lowerText.contains("高兴") || lowerText.contains("快乐")) {
        return "happy";
    } else if (lowerText.contains("难过") || lowerText.contains("伤心") || lowerText.contains("悲伤")) {
        return "sad";
    } else if (lowerText.contains("生气") || lowerText.contains("愤怒") || lowerText.contains("恼火")) {
        return "angry";
    } else if (lowerText.contains("兴奋") || lowerText.contains("激动") || lowerText.contains("兴奋")) {
        return "excited";
    } else if (lowerText.contains("困") || lowerText.contains("累") || lowerText.contains("疲惫")) {
        return "sleepy";
    }
    
    return QString();
}

AnimationType DeskPetStateManager::getAnimationForBehavior(PetBehavior behavior)
{
    switch (behavior) {
    case PetBehavior::IDLE:
        return AnimationType::IDLE_LOOP;
    case PetBehavior::LISTENING:
        return AnimationType::LISTENING;
    case PetBehavior::SPEAKING:
        return AnimationType::SPEAKING;
    case PetBehavior::THINKING:
        return AnimationType::THINKING;
    case PetBehavior::EXCITED:
        return AnimationType::EXCITED;
    case PetBehavior::SAD:
        return AnimationType::SAD;
    case PetBehavior::ANGRY:
        return AnimationType::ANGRY;
    case PetBehavior::SLEEPING:
        return AnimationType::SLEEPING;
    default:
        return AnimationType::IDLE_LOOP;
    }
}

AnimationType DeskPetStateManager::getAnimationForEmotion(const QString &emotion)
{
    if (emotion == "happy" || emotion == "excited") {
        return AnimationType::EXCITED;
    } else if (emotion == "sad") {
        return AnimationType::SAD;
    } else if (emotion == "angry") {
        return AnimationType::ANGRY;
    } else if (emotion == "sleepy") {
        return AnimationType::SLEEPING;
    }
    
    return AnimationType::IDLE_LOOP;
}

void DeskPetStateManager::transitionToIdle()
{
    setBehavior(PetBehavior::IDLE);
    setAudioState(AudioState::SILENT);
}

void DeskPetStateManager::transitionToListening()
{
    setBehavior(PetBehavior::LISTENING);
    setAudioState(AudioState::RECORDING);
}

void DeskPetStateManager::transitionToSpeaking()
{
    setBehavior(PetBehavior::SPEAKING);
    setAudioState(AudioState::PLAYING);
}

void DeskPetStateManager::transitionToThinking()
{
    setBehavior(PetBehavior::THINKING);
    setAudioState(AudioState::PROCESSING);
}

void DeskPetStateManager::transitionToExcited()
{
    setBehavior(PetBehavior::EXCITED);
}

void DeskPetStateManager::transitionToSad()
{
    setBehavior(PetBehavior::SAD);
}

void DeskPetStateManager::transitionToAngry()
{
    setBehavior(PetBehavior::ANGRY);
}

void DeskPetStateManager::transitionToSleeping()
{
    setBehavior(PetBehavior::SLEEPING);
}
