//
// 异步状态管理器实现
// 参考py-xiaozhi的状态管理，实现异步状态更新
//

#include "AsyncStateManager.h"
#include "ThreadSafeUIUpdater.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonObject>

AsyncStateManager::AsyncStateManager(QObject *parent)
    : QObject(parent)
    , m_deviceState("idle")
    , m_petBehavior("idle")
    , m_emotion("neutral")
    , m_animation("idle")
    , m_audioState("silent")
    , m_connected(false)
    , m_processTimer(nullptr)
    , m_uiUpdater(nullptr)
{
    initializeTimer();
    qDebug() << "AsyncStateManager created";
}

AsyncStateManager::~AsyncStateManager()
{
    if (m_processTimer) {
        m_processTimer->stop();
        m_processTimer->deleteLater();
    }
    qDebug() << "AsyncStateManager destroyed";
}

void AsyncStateManager::setDeviceState(const QString& state)
{
    if (m_deviceState == state) {
        return;
    }
    
    StateChange change(StateType::DEVICE_STATE, m_deviceState, state);
    processStateChange(change);
}

void AsyncStateManager::setPetBehavior(const QString& behavior)
{
    if (m_petBehavior == behavior) {
        return;
    }
    
    StateChange change(StateType::PET_BEHAVIOR, m_petBehavior, behavior);
    processStateChange(change);
}

void AsyncStateManager::setEmotion(const QString& emotion)
{
    if (m_emotion == emotion) {
        return;
    }
    
    StateChange change(StateType::EMOTION_STATE, m_emotion, emotion);
    processStateChange(change);
}

void AsyncStateManager::setAnimation(const QString& animation)
{
    if (m_animation == animation) {
        return;
    }
    
    StateChange change(StateType::ANIMATION_STATE, m_animation, animation);
    processStateChange(change);
}

void AsyncStateManager::setAudioState(const QString& state)
{
    if (m_audioState == state) {
        return;
    }
    
    StateChange change(StateType::AUDIO_STATE, m_audioState, state);
    processStateChange(change);
}

void AsyncStateManager::setConnectionState(bool connected)
{
    if (m_connected == connected) {
        return;
    }
    
    StateChange change(StateType::CONNECTION_STATE, 
                      m_connected ? "connected" : "disconnected",
                      connected ? "connected" : "disconnected");
    processStateChange(change);
}

QString AsyncStateManager::getDeviceState() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_deviceState;
}

QString AsyncStateManager::getPetBehavior() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_petBehavior;
}

QString AsyncStateManager::getEmotion() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_emotion;
}

QString AsyncStateManager::getAnimation() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_animation;
}

QString AsyncStateManager::getAudioState() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_audioState;
}

bool AsyncStateManager::isConnected() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_connected;
}

StateSnapshot AsyncStateManager::getStateSnapshot() const
{
    QMutexLocker locker(&m_stateMutex);
    
    StateSnapshot snapshot;
    snapshot.deviceState = m_deviceState;
    snapshot.petBehavior = m_petBehavior;
    snapshot.emotion = m_emotion;
    snapshot.animation = m_animation;
    snapshot.audioState = m_audioState;
    snapshot.connected = m_connected;
    snapshot.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    return snapshot;
}

void AsyncStateManager::processStateChange(const StateChange& change)
{
    // 验证状态变化
    if (!canTransitionTo(change.newValue, change.type)) {
        QString error = QString("Invalid state transition: %1 -> %2")
                       .arg(change.oldValue, change.newValue);
        emit transitionError(change.oldValue, change.newValue, error);
        return;
    }
    
    // 添加到待处理队列
    QMutexLocker locker(&m_queueMutex);
    m_pendingChanges.enqueue(change);
    
    qDebug() << "State change queued:" << static_cast<int>(change.type) 
             << change.oldValue << "->" << change.newValue;
}

void AsyncStateManager::processPendingChanges()
{
    QMutexLocker locker(&m_queueMutex);
    
    if (m_pendingChanges.isEmpty()) {
        return;
    }
    
    // 批量处理状态变化
    int processedCount = 0;
    const int maxBatchSize = 10;
    
    while (!m_pendingChanges.isEmpty() && processedCount < maxBatchSize) {
        StateChange change = m_pendingChanges.dequeue();
        processSingleChange(change);
        processedCount++;
    }
}

bool AsyncStateManager::isValidState(const QString& state, StateType type) const
{
    switch (type) {
    case StateType::DEVICE_STATE:
        return state == "idle" || state == "listening" || state == "speaking" || 
               state == "processing" || state == "error";
    case StateType::PET_BEHAVIOR:
        return state == "idle" || state == "listening" || state == "speaking" || 
               state == "thinking" || state == "excited" || state == "sad" || 
               state == "angry" || state == "sleeping";
    case StateType::EMOTION_STATE:
        return state == "neutral" || state == "happy" || state == "sad" || 
               state == "angry" || state == "excited" || state == "confused";
    case StateType::ANIMATION_STATE:
        return state == "idle" || state == "listening" || state == "speaking" || 
               state == "thinking" || state == "excited" || state == "sad" || 
               state == "angry" || state == "sleeping";
    case StateType::AUDIO_STATE:
        return state == "silent" || state == "recording" || state == "playing" || 
               state == "processing";
    case StateType::CONNECTION_STATE:
        return state == "connected" || state == "disconnected" || state == "connecting";
    default:
        return false;
    }
}

bool AsyncStateManager::canTransitionTo(const QString& newState, StateType type) const
{
    if (!isValidState(newState, type)) {
        return false;
    }
    
    switch (type) {
    case StateType::DEVICE_STATE:
        return canTransitionDeviceState(getDeviceState(), newState);
    case StateType::PET_BEHAVIOR:
        return canTransitionPetBehavior(getPetBehavior(), newState);
    case StateType::EMOTION_STATE:
        return canTransitionEmotion(getEmotion(), newState);
    case StateType::ANIMATION_STATE:
        return canTransitionAnimation(getAnimation(), newState);
    case StateType::AUDIO_STATE:
        return canTransitionAudioState(getAudioState(), newState);
    case StateType::CONNECTION_STATE:
        return true; // 连接状态可以自由转换
    default:
        return false;
    }
}

void AsyncStateManager::batchUpdate(const QJsonObject& updates)
{
    QMutexLocker locker(&m_queueMutex);
    
    // 批量处理多个状态更新
    for (auto it = updates.begin(); it != updates.end(); ++it) {
        QString key = it.key();
        QString value = it.value().toString();
        
        if (key == "deviceState") {
            setDeviceState(value);
        } else if (key == "petBehavior") {
            setPetBehavior(value);
        } else if (key == "emotion") {
            setEmotion(value);
        } else if (key == "animation") {
            setAnimation(value);
        } else if (key == "audioState") {
            setAudioState(value);
        } else if (key == "connected") {
            setConnectionState(value == "true");
        }
    }
    
    qDebug() << "Batch update processed";
}

void AsyncStateManager::clearStateHistory()
{
    QMutexLocker locker(&m_queueMutex);
    m_stateHistory.clear();
    m_pendingChanges.clear();
    qDebug() << "State history cleared";
}

void AsyncStateManager::addStateListener(StateType type, QObject* listener, const char* slot)
{
    // 这里可以实现状态监听机制
    Q_UNUSED(type)
    Q_UNUSED(listener)
    Q_UNUSED(slot)
    qDebug() << "State listener added";
}

void AsyncStateManager::removeStateListener(StateType type, QObject* listener)
{
    // 这里可以实现状态监听移除机制
    Q_UNUSED(type)
    Q_UNUSED(listener)
    qDebug() << "State listener removed";
}

void AsyncStateManager::processStateQueue()
{
    processPendingChanges();
}

void AsyncStateManager::onStateChangeProcessed(const StateChange& change)
{
    qDebug() << "State change processed:" << static_cast<int>(change.type) 
             << change.oldValue << "->" << change.newValue;
}

void AsyncStateManager::initializeTimer()
{
    m_processTimer = new QTimer(this);
    m_processTimer->setInterval(PROCESS_INTERVAL);
    connect(m_processTimer, &QTimer::timeout, this, &AsyncStateManager::processStateQueue);
    m_processTimer->start();
    
    qDebug() << "AsyncStateManager timer initialized";
}

void AsyncStateManager::processSingleChange(const StateChange& change)
{
    // 更新状态
    QMutexLocker locker(&m_stateMutex);
    
    switch (change.type) {
    case StateType::DEVICE_STATE:
        m_deviceState = change.newValue;
        break;
    case StateType::PET_BEHAVIOR:
        m_petBehavior = change.newValue;
        break;
    case StateType::EMOTION_STATE:
        m_emotion = change.newValue;
        break;
    case StateType::ANIMATION_STATE:
        m_animation = change.newValue;
        break;
    case StateType::AUDIO_STATE:
        m_audioState = change.newValue;
        break;
    case StateType::CONNECTION_STATE:
        m_connected = (change.newValue == "connected");
        break;
    }
    
    // 添加到历史记录
    m_stateHistory.enqueue(change);
    if (m_stateHistory.size() > MAX_HISTORY_SIZE) {
        m_stateHistory.dequeue();
    }
    
    // 更新UI
    updateUIState(change);
    
    // 发出状态变化信号
    notifyStateChange(change);
    
    // 更新状态快照
    updateStateSnapshot();
}

void AsyncStateManager::updateUIState(const StateChange& change)
{
    if (!m_uiUpdater) {
        return;
    }
    
    switch (change.type) {
    case StateType::DEVICE_STATE:
        m_uiUpdater->updateStatus(change.newValue);
        break;
    case StateType::PET_BEHAVIOR:
        m_uiUpdater->updatePetAnimation(change.newValue);
        break;
    case StateType::EMOTION_STATE:
        m_uiUpdater->updatePetEmotion(change.newValue);
        break;
    case StateType::ANIMATION_STATE:
        m_uiUpdater->updatePetAnimation(change.newValue);
        break;
    case StateType::AUDIO_STATE:
        // 音频状态变化通常不需要UI更新
        break;
    case StateType::CONNECTION_STATE:
        m_uiUpdater->updateConnectionStatus(change.newValue == "connected");
        break;
    }
}

void AsyncStateManager::validateStateChange(const StateChange& change)
{
    if (!isValidState(change.newValue, change.type)) {
        QString error = QString("Invalid state: %1 for type: %2")
                       .arg(change.newValue).arg(static_cast<int>(change.type));
        emit stateError(error);
    }
}

void AsyncStateManager::notifyStateChange(const StateChange& change)
{
    switch (change.type) {
    case StateType::DEVICE_STATE:
        emit deviceStateChanged(change.oldValue, change.newValue);
        break;
    case StateType::PET_BEHAVIOR:
        emit petBehaviorChanged(change.oldValue, change.newValue);
        break;
    case StateType::EMOTION_STATE:
        emit emotionChanged(change.oldValue, change.newValue);
        break;
    case StateType::ANIMATION_STATE:
        emit animationChanged(change.oldValue, change.newValue);
        break;
    case StateType::AUDIO_STATE:
        emit audioStateChanged(change.oldValue, change.newValue);
        break;
    case StateType::CONNECTION_STATE:
        emit connectionStateChanged(change.oldValue == "connected", change.newValue == "connected");
        break;
    }
}

void AsyncStateManager::updateStateSnapshot()
{
    StateSnapshot snapshot = getStateSnapshot();
    emit stateSnapshotUpdated(snapshot);
}

// 状态转换规则实现
bool AsyncStateManager::canTransitionDeviceState(const QString& from, const QString& to) const
{
    // 设备状态转换规则
    if (from == "idle") {
        return to == "listening" || to == "error";
    } else if (from == "listening") {
        return to == "processing" || to == "idle" || to == "error";
    } else if (from == "processing") {
        return to == "speaking" || to == "idle" || to == "error";
    } else if (from == "speaking") {
        return to == "idle" || to == "error";
    } else if (from == "error") {
        return to == "idle";
    }
    return false;
}

bool AsyncStateManager::canTransitionPetBehavior(const QString& from, const QString& to) const
{
    // 宠物行为转换规则
    if (from == "idle") {
        return to == "listening" || to == "sleeping" || to == "excited";
    } else if (from == "listening") {
        return to == "thinking" || to == "idle";
    } else if (from == "thinking") {
        return to == "speaking" || to == "idle";
    } else if (from == "speaking") {
        return to == "idle" || to == "excited";
    } else if (from == "sleeping") {
        return to == "idle";
    }
    return false;
}

bool AsyncStateManager::canTransitionEmotion(const QString& from, const QString& to) const
{
    // 情绪转换规则 - 情绪可以自由转换
    return true;
}

bool AsyncStateManager::canTransitionAnimation(const QString& from, const QString& to) const
{
    // 动画转换规则 - 动画可以自由转换
    return true;
}

bool AsyncStateManager::canTransitionAudioState(const QString& from, const QString& to) const
{
    // 音频状态转换规则
    if (from == "silent") {
        return to == "recording" || to == "playing";
    } else if (from == "recording") {
        return to == "processing" || to == "silent";
    } else if (from == "processing") {
        return to == "playing" || to == "silent";
    } else if (from == "playing") {
        return to == "silent";
    }
    return false;
}

