//
// 线程安全的UI更新器实现
// 确保UI更新在UI线程中执行，避免线程安全问题
//

#include "ThreadSafeUIUpdater.h"
#include "WebSocketChatDialog.h"
#include "DeskPetIntegration.h"
// #include "LAppLive2DManager.h"  // 前向声明就够了
#include <QDebug>
#include <QThread>
#include <QApplication>

ThreadSafeUIUpdater::ThreadSafeUIUpdater(QObject *parent)
    : QObject(parent)
    , m_chatDialog(nullptr)
    , m_deskPetIntegration(nullptr)
    , m_live2DManager(nullptr)
    , m_processTimer(nullptr)
{
    initializeTimer();
    qDebug() << "ThreadSafeUIUpdater created";
}

ThreadSafeUIUpdater::~ThreadSafeUIUpdater()
{
    if (m_processTimer) {
        m_processTimer->stop();
        m_processTimer->deleteLater();
    }
    qDebug() << "ThreadSafeUIUpdater destroyed";
}

void ThreadSafeUIUpdater::setChatDialog(WebSocketChatDialog* dialog)
{
    m_chatDialog = dialog;
    qDebug() << "Chat dialog set";
}

void ThreadSafeUIUpdater::setDeskPetIntegration(DeskPetIntegration* integration)
{
    m_deskPetIntegration = integration;
    qDebug() << "DeskPet integration set";
}

void ThreadSafeUIUpdater::setLive2DManager(LAppLive2DManager* manager)
{
    m_live2DManager = manager;
    qDebug() << "Live2D manager set";
}

void ThreadSafeUIUpdater::updateChatMessage(const QString& message)
{
    UIUpdateMessage update(UIUpdateType::CHAT_MESSAGE, message);
    scheduleUIUpdate(update);
}

void ThreadSafeUIUpdater::updatePetAnimation(const QString& animation)
{
    UIUpdateMessage update;
    update.type = UIUpdateType::PET_ANIMATION;
    update.animation = animation;
    scheduleUIUpdate(update);
}

void ThreadSafeUIUpdater::updatePetEmotion(const QString& emotion)
{
    UIUpdateMessage update;
    update.type = UIUpdateType::PET_EMOTION;
    update.emotion = emotion;
    scheduleUIUpdate(update);
}

void ThreadSafeUIUpdater::updateStatus(const QString& status)
{
    UIUpdateMessage update(UIUpdateType::STATUS_UPDATE, status);
    scheduleUIUpdate(update);
}

void ThreadSafeUIUpdater::playAudioSafely(const QByteArray& audioData)
{
    UIUpdateMessage update(UIUpdateType::AUDIO_PLAYBACK, audioData);
    scheduleUIUpdate(update);
}

void ThreadSafeUIUpdater::updateConnectionStatus(bool connected)
{
    UIUpdateMessage update;
    update.type = UIUpdateType::CONNECTION_STATUS;
    update.content = connected ? "connected" : "disconnected";
    scheduleUIUpdate(update);
}

void ThreadSafeUIUpdater::showErrorMessage(const QString& error)
{
    UIUpdateMessage update;
    update.type = UIUpdateType::ERROR_MESSAGE;
    update.content = error;
    update.isError = true;
    scheduleUIUpdate(update);
}

void ThreadSafeUIUpdater::scheduleUIUpdate(const UIUpdateMessage& update)
{
    QMutexLocker locker(&m_queueMutex);
    
    if (m_updateQueue.size() >= MAX_QUEUE_SIZE) {
        qWarning() << "UI update queue full, dropping oldest update";
        m_updateQueue.dequeue();
    }
    
    m_updateQueue.enqueue(update);
    qDebug() << "UI update scheduled, type:" << static_cast<int>(update.type);
}

void ThreadSafeUIUpdater::processPendingUpdates()
{
    QMutexLocker locker(&m_queueMutex);
    
    if (m_updateQueue.isEmpty()) {
        return;
    }
    
    // 批量处理更新，保持60fps
    int processedCount = 0;
    const int maxBatchSize = 5;
    
    while (!m_updateQueue.isEmpty() && processedCount < maxBatchSize) {
        UIUpdateMessage update = m_updateQueue.dequeue();
        processSingleUpdate(update);
        processedCount++;
    }
}

bool ThreadSafeUIUpdater::hasPendingUpdates() const
{
    QMutexLocker locker(&m_queueMutex);
    return !m_updateQueue.isEmpty();
}

int ThreadSafeUIUpdater::pendingUpdateCount() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_updateQueue.size();
}

void ThreadSafeUIUpdater::clearPendingUpdates()
{
    QMutexLocker locker(&m_queueMutex);
    m_updateQueue.clear();
    qDebug() << "Pending UI updates cleared";
}

void ThreadSafeUIUpdater::initializeTimer()
{
    m_processTimer = new QTimer(this);
    m_processTimer->setInterval(UPDATE_PROCESS_INTERVAL);
    connect(m_processTimer, &QTimer::timeout, this, &ThreadSafeUIUpdater::processUpdateQueue);
    m_processTimer->start();
    
    qDebug() << "ThreadSafeUIUpdater timer initialized";
}

void ThreadSafeUIUpdater::processUpdateQueue()
{
    processPendingUpdates();
}

void ThreadSafeUIUpdater::handleUpdateProcessed(const UIUpdateMessage& update)
{
    qDebug() << "UI update processed, type:" << static_cast<int>(update.type);
}

void ThreadSafeUIUpdater::processSingleUpdate(const UIUpdateMessage& update)
{
    ensureUIThread();
    
    qDebug() << "Processing UI update, type:" << static_cast<int>(update.type);
    
    switch (update.type) {
    case UIUpdateType::CHAT_MESSAGE:
        updateChatDialog(update.content);
        break;
    case UIUpdateType::PET_ANIMATION:
        updatePetAnimationInternal(update.animation);
        break;
    case UIUpdateType::PET_EMOTION:
        updatePetEmotionInternal(update.emotion);
        break;
    case UIUpdateType::STATUS_UPDATE:
        updateStatusInternal(update.content);
        break;
    case UIUpdateType::AUDIO_PLAYBACK:
        playAudioInternal(update.audioData);
        break;
    case UIUpdateType::CONNECTION_STATUS:
        updateConnectionStatusInternal(update.content == "connected");
        break;
    case UIUpdateType::ERROR_MESSAGE:
        showErrorInternal(update.content);
        break;
    default:
        qWarning() << "Unknown UI update type:" << static_cast<int>(update.type);
        break;
    }
    
    emit updateProcessed(update);
}

void ThreadSafeUIUpdater::updateChatDialog(const QString& message)
{
    if (!m_chatDialog) {
        qWarning() << "Chat dialog not set, cannot update message";
        return;
    }
    
    // 使用Qt的元对象系统确保在UI线程中执行
    QMetaObject::invokeMethod(m_chatDialog, "BotReply", 
                              Qt::QueuedConnection,
                              Q_ARG(QString, message));
    
    emit chatMessageUpdated(message);
    qDebug() << "Chat dialog updated with message:" << message;
}

void ThreadSafeUIUpdater::updatePetAnimationInternal(const QString& animation)
{
    if (!m_live2DManager) {
        qWarning() << "Live2D manager not set, cannot update animation";
        return;
    }
    
    // 这里可以调用Live2D的动画播放方法
    // 例如：m_live2DManager->playAnimation(animation);
    qDebug() << "Pet animation updated:" << animation;
    
    emit petAnimationUpdated(animation);
}

void ThreadSafeUIUpdater::updatePetEmotionInternal(const QString& emotion)
{
    if (!m_live2DManager) {
        qWarning() << "Live2D manager not set, cannot update emotion";
        return;
    }
    
    // 这里可以调用Live2D的表情更新方法
    // 例如：m_live2DManager->setExpression(emotion);
    qDebug() << "Pet emotion updated:" << emotion;
    
    emit petEmotionUpdated(emotion);
}

void ThreadSafeUIUpdater::updateStatusInternal(const QString& status)
{
    qDebug() << "Status updated:" << status;
    emit statusUpdated(status);
}

void ThreadSafeUIUpdater::playAudioInternal(const QByteArray& audioData)
{
    if (!m_deskPetIntegration) {
        qWarning() << "DeskPet integration not set, cannot play audio";
        return;
    }
    
    // 使用DeskPetIntegration的音频播放功能
    m_deskPetIntegration->playAudioData(audioData);
    qDebug() << "Audio playback requested, size:" << audioData.size();
    
    emit audioPlaybackRequested(audioData);
}

void ThreadSafeUIUpdater::updateConnectionStatusInternal(bool connected)
{
    qDebug() << "Connection status updated:" << connected;
    emit connectionStatusUpdated(connected);
}

void ThreadSafeUIUpdater::showErrorInternal(const QString& error)
{
    qCritical() << "Error message:" << error;
    emit errorMessageShown(error);
}

bool ThreadSafeUIUpdater::isUIThread() const
{
    return QThread::currentThread() == QApplication::instance()->thread();
}

void ThreadSafeUIUpdater::ensureUIThread()
{
    if (!isUIThread()) {
        qWarning() << "UI update called from non-UI thread, this may cause issues";
    }
}

