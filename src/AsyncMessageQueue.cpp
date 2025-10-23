//
// 异步消息队列系统实现
// 用于将对话处理从UI线程分离，提升渲染性能
//

#include "AsyncMessageQueue.h"
#include <QDebug>
#include <QDateTime>

AsyncMessageQueue::AsyncMessageQueue(QObject *parent)
    : QObject(parent)
    , m_messageTimer(nullptr)
    , m_uiTimer(nullptr)
{
    initializeTimers();
    qDebug() << "AsyncMessageQueue initialized";
}

AsyncMessageQueue::~AsyncMessageQueue()
{
    if (m_messageTimer) {
        m_messageTimer->stop();
        m_messageTimer->deleteLater();
    }
    if (m_uiTimer) {
        m_uiTimer->stop();
        m_uiTimer->deleteLater();
    }
    qDebug() << "AsyncMessageQueue destroyed";
}

void AsyncMessageQueue::initializeTimers()
{
    // 消息处理定时器
    m_messageTimer = new QTimer(this);
    m_messageTimer->setInterval(MESSAGE_PROCESS_INTERVAL);
    connect(m_messageTimer, &QTimer::timeout, this, &AsyncMessageQueue::processMessageQueue);
    m_messageTimer->start();
    
    // UI更新定时器
    m_uiTimer = new QTimer(this);
    m_uiTimer->setInterval(UI_UPDATE_INTERVAL);
    connect(m_uiTimer, &QTimer::timeout, this, &AsyncMessageQueue::processUIUpdates);
    m_uiTimer->start();
    
    qDebug() << "AsyncMessageQueue timers initialized";
}

void AsyncMessageQueue::enqueueMessage(const AsyncMessage& message)
{
    QMutexLocker locker(&m_messageMutex);
    
    if (m_messageQueue.size() >= MAX_QUEUE_SIZE) {
        qWarning() << "Message queue overflow, dropping oldest message";
        m_messageQueue.dequeue();
        emit queueOverflow();
    }
    
    m_messageQueue.enqueue(message);
    emit queueSizeChanged(m_messageQueue.size());
    
    qDebug() << "Message enqueued, type:" << static_cast<int>(message.type) 
             << "queue size:" << m_messageQueue.size();
}

void AsyncMessageQueue::enqueueTextMessage(const QString& text)
{
    AsyncMessage message;
    message.type = AsyncMessageType::TEXT_MESSAGE;
    message.text = text;
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    enqueueMessage(message);
}

void AsyncMessageQueue::enqueueAudioMessage(const QByteArray& audioData)
{
    AsyncMessage message;
    message.type = AsyncMessageType::AUDIO_MESSAGE;
    message.audioData = audioData;
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    enqueueMessage(message);
}

void AsyncMessageQueue::enqueueStateUpdate(const QString& state)
{
    AsyncMessage message;
    message.type = AsyncMessageType::STATE_UPDATE;
    message.text = state;
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    enqueueMessage(message);
}

void AsyncMessageQueue::enqueueEmotionUpdate(const QString& emotion)
{
    AsyncMessage message;
    message.type = AsyncMessageType::EMOTION_UPDATE;
    message.emotion = emotion;
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    enqueueMessage(message);
}

void AsyncMessageQueue::enqueueAnimationRequest(const QString& animation)
{
    AsyncMessage message;
    message.type = AsyncMessageType::ANIMATION_REQUEST;
    message.animation = animation;
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    enqueueMessage(message);
}

void AsyncMessageQueue::scheduleUIUpdate(const UIUpdate& update)
{
    QMutexLocker locker(&m_uiMutex);
    
    if (m_uiUpdateQueue.size() >= MAX_QUEUE_SIZE) {
        qWarning() << "UI update queue overflow, dropping oldest update";
        m_uiUpdateQueue.dequeue();
    }
    
    m_uiUpdateQueue.enqueue(update);
    
    qDebug() << "UI update scheduled, type:" << static_cast<int>(update.type);
}

bool AsyncMessageQueue::hasMessages() const
{
    QMutexLocker locker(&m_messageMutex);
    return !m_messageQueue.isEmpty();
}

int AsyncMessageQueue::queueSize() const
{
    QMutexLocker locker(&m_messageMutex);
    return m_messageQueue.size();
}

void AsyncMessageQueue::clearQueue()
{
    QMutexLocker messageLocker(&m_messageMutex);
    QMutexLocker uiLocker(&m_uiMutex);
    
    m_messageQueue.clear();
    m_uiUpdateQueue.clear();
    
    qDebug() << "Message queues cleared";
}

void AsyncMessageQueue::processMessageQueue()
{
    QMutexLocker locker(&m_messageMutex);
    
    if (m_messageQueue.isEmpty()) {
        return;
    }
    
    // 批量处理消息，避免阻塞
    int processedCount = 0;
    const int maxBatchSize = 10;
    
    while (!m_messageQueue.isEmpty() && processedCount < maxBatchSize) {
        AsyncMessage message = m_messageQueue.dequeue();
        processSingleMessage(message);
        processedCount++;
    }
    
    emit queueSizeChanged(m_messageQueue.size());
}

void AsyncMessageQueue::processUIUpdates()
{
    QMutexLocker locker(&m_uiMutex);
    
    if (m_uiUpdateQueue.isEmpty()) {
        return;
    }
    
    // 批量处理UI更新，保持60fps
    int processedCount = 0;
    const int maxBatchSize = 5;
    
    while (!m_uiUpdateQueue.isEmpty() && processedCount < maxBatchSize) {
        UIUpdate update = m_uiUpdateQueue.dequeue();
        processSingleUIUpdate(update);
        processedCount++;
    }
}

void AsyncMessageQueue::processSingleMessage(const AsyncMessage& message)
{
    qDebug() << "Processing message, type:" << static_cast<int>(message.type);
    
    // 根据消息类型处理
    switch (message.type) {
    case AsyncMessageType::TEXT_MESSAGE:
        qDebug() << "Text message:" << message.text;
        break;
    case AsyncMessageType::AUDIO_MESSAGE:
        qDebug() << "Audio message, size:" << message.audioData.size();
        break;
    case AsyncMessageType::STATE_UPDATE:
        qDebug() << "State update:" << message.text;
        break;
    case AsyncMessageType::EMOTION_UPDATE:
        qDebug() << "Emotion update:" << message.emotion;
        break;
    case AsyncMessageType::ANIMATION_REQUEST:
        qDebug() << "Animation request:" << message.animation;
        break;
    default:
        qDebug() << "Unknown message type:" << static_cast<int>(message.type);
        break;
    }
    
    // 发出消息处理信号
    emit messageReady(message);
}

void AsyncMessageQueue::processSingleUIUpdate(const UIUpdate& update)
{
    qDebug() << "Processing UI update, type:" << static_cast<int>(update.type);
    
    // 发出UI更新信号
    emit uiUpdateReady(update);
}

