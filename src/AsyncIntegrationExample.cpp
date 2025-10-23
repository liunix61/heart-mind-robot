//
// 异步组件集成示例实现
// 展示如何在DeskPetIntegration中使用异步组件
//

#include "AsyncIntegrationExample.h"
#include "WebSocketChatDialog.h"
#include "DeskPetIntegration.h"
// #include "LAppLive2DManager.h"  // 前向声明就够了
#include <QDebug>

AsyncIntegrationExample::AsyncIntegrationExample(QObject *parent)
    : QObject(parent)
    , m_messageQueue(nullptr)
    , m_conversationWorker(nullptr)
    , m_uiUpdater(nullptr)
    , m_stateManager(nullptr)
    , m_chatDialog(nullptr)
    , m_integration(nullptr)
    , m_live2DManager(nullptr)
    , m_initialized(false)
    , m_connected(false)
{
    logInfo("AsyncIntegrationExample created");
}

AsyncIntegrationExample::~AsyncIntegrationExample()
{
    shutdown();
    logInfo("AsyncIntegrationExample destroyed");
}

bool AsyncIntegrationExample::initialize(WebSocketChatDialog* chatDialog,
                                        DeskPetIntegration* integration,
                                        LAppLive2DManager* live2DManager)
{
    if (m_initialized) {
        logError("Already initialized");
        return false;
    }
    
    logInfo("Initializing async components...");
    
    // 保存组件引用
    m_chatDialog = chatDialog;
    m_integration = integration;
    m_live2DManager = live2DManager;
    
    try {
        // 创建异步组件
        m_messageQueue = new AsyncMessageQueue(this);
        m_conversationWorker = new ConversationWorker(this);
        m_uiUpdater = new ThreadSafeUIUpdater(this);
        m_stateManager = new AsyncStateManager(this);
        
        // 设置UI组件引用
        m_uiUpdater->setChatDialog(chatDialog);
        m_uiUpdater->setDeskPetIntegration(integration);
        m_uiUpdater->setLive2DManager(live2DManager);
        
        // 建立信号槽连接
        setupConnections();
        
        m_initialized = true;
        logInfo("Async components initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError(QString("Failed to initialize: %1").arg(e.what()));
        return false;
    }
}

bool AsyncIntegrationExample::connectToServer(const QString& serverUrl,
                                             const QString& accessToken,
                                             const QString& deviceId)
{
    if (!m_initialized) {
        logError("Not initialized, cannot connect to server");
        return false;
    }
    
    if (m_connected) {
        logInfo("Already connected to server");
        return true;
    }
    
    logInfo(QString("Connecting to server: %1").arg(serverUrl));
    
    // 配置工作线程
    m_conversationWorker->setServerUrl(serverUrl);
    m_conversationWorker->setAccessToken(accessToken);
    m_conversationWorker->setDeviceId(deviceId);
    
    // 启动工作线程
    m_conversationWorker->startWorker();
    
    // 连接到服务器
    m_conversationWorker->connectToServer();
    
    return true;
}

void AsyncIntegrationExample::disconnectFromServer()
{
    if (!m_connected) {
        return;
    }
    
    logInfo("Disconnecting from server...");
    
    if (m_conversationWorker) {
        m_conversationWorker->disconnectFromServer();
    }
    
    m_connected = false;
}

bool AsyncIntegrationExample::isConnected() const
{
    return m_connected && m_conversationWorker && m_conversationWorker->isConnected();
}

void AsyncIntegrationExample::sendTextMessage(const QString& text)
{
    if (!isConnected()) {
        logError("Not connected, cannot send message");
        m_uiUpdater->showErrorMessage("未连接到服务器");
        return;
    }
    
    logDebug(QString("Sending text message: %1").arg(text));
    
    // 异步发送消息，不阻塞UI线程
    m_conversationWorker->processTextMessage(text);
    
    // 更新状态
    m_stateManager->setDeviceState("listening");
}

void AsyncIntegrationExample::sendAudioMessage(const QByteArray& audioData)
{
    if (!isConnected()) {
        logError("Not connected, cannot send audio");
        return;
    }
    
    logDebug(QString("Sending audio message, size: %1 bytes").arg(audioData.size()));
    
    // 异步发送音频消息
    m_conversationWorker->processAudioMessage(audioData);
}

void AsyncIntegrationExample::updatePetBehavior(const QString& behavior)
{
    logDebug(QString("Updating pet behavior: %1").arg(behavior));
    m_stateManager->setPetBehavior(behavior);
}

void AsyncIntegrationExample::updateEmotion(const QString& emotion)
{
    logDebug(QString("Updating emotion: %1").arg(emotion));
    m_stateManager->setEmotion(emotion);
}

void AsyncIntegrationExample::updateAnimation(const QString& animation)
{
    logDebug(QString("Updating animation: %1").arg(animation));
    m_stateManager->setAnimation(animation);
}

void AsyncIntegrationExample::updateDeviceState(const QString& state)
{
    logDebug(QString("Updating device state: %1").arg(state));
    m_stateManager->setDeviceState(state);
}

QString AsyncIntegrationExample::getCurrentBehavior() const
{
    return m_stateManager ? m_stateManager->getPetBehavior() : QString();
}

QString AsyncIntegrationExample::getCurrentEmotion() const
{
    return m_stateManager ? m_stateManager->getEmotion() : QString();
}

QString AsyncIntegrationExample::getCurrentDeviceState() const
{
    return m_stateManager ? m_stateManager->getDeviceState() : QString();
}

StateSnapshot AsyncIntegrationExample::getStateSnapshot() const
{
    return m_stateManager ? m_stateManager->getStateSnapshot() : StateSnapshot();
}

void AsyncIntegrationExample::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    logInfo("Shutting down async components...");
    
    // 断开连接
    disconnectFromServer();
    
    // 停止工作线程
    if (m_conversationWorker) {
        m_conversationWorker->stopWorker();
    }
    
    // 清理资源
    if (m_messageQueue) {
        m_messageQueue->clearQueue();
    }
    
    if (m_stateManager) {
        m_stateManager->clearStateHistory();
    }
    
    m_initialized = false;
    logInfo("Async components shutdown complete");
}

// 建立信号槽连接
void AsyncIntegrationExample::setupConnections()
{
    setupWorkerConnections();
    setupStateManagerConnections();
    setupUIUpdaterConnections();
    setupMessageQueueConnections();
    
    logDebug("All connections established");
}

void AsyncIntegrationExample::setupWorkerConnections()
{
    if (!m_conversationWorker) return;
    
    // 连接状态
    connect(m_conversationWorker, &ConversationWorker::connected,
            this, &AsyncIntegrationExample::onWorkerConnected);
    connect(m_conversationWorker, &ConversationWorker::disconnected,
            this, &AsyncIntegrationExample::onWorkerDisconnected);
    connect(m_conversationWorker, &ConversationWorker::connectionError,
            this, &AsyncIntegrationExample::onWorkerError);
    
    // 消息处理
    connect(m_conversationWorker, &ConversationWorker::responseReady,
            this, &AsyncIntegrationExample::onWorkerResponseReady);
    connect(m_conversationWorker, &ConversationWorker::audioResponseReady,
            this, &AsyncIntegrationExample::onWorkerAudioReady);
    
    // 宠物状态
    connect(m_conversationWorker, &ConversationWorker::petEmotionChanged,
            this, &AsyncIntegrationExample::onWorkerEmotionChanged);
    connect(m_conversationWorker, &ConversationWorker::petAnimationRequested,
            this, &AsyncIntegrationExample::onWorkerAnimationRequested);
    
    // UI更新 - 将工作线程的响应转发到UI更新器
    connect(m_conversationWorker, &ConversationWorker::responseReady,
            m_uiUpdater, &ThreadSafeUIUpdater::updateChatMessage);
    connect(m_conversationWorker, &ConversationWorker::audioResponseReady,
            m_uiUpdater, &ThreadSafeUIUpdater::playAudioSafely);
    connect(m_conversationWorker, &ConversationWorker::petEmotionChanged,
            m_uiUpdater, &ThreadSafeUIUpdater::updatePetEmotion);
    connect(m_conversationWorker, &ConversationWorker::petAnimationRequested,
            m_uiUpdater, &ThreadSafeUIUpdater::updatePetAnimation);
    
    logDebug("Worker connections established");
}

void AsyncIntegrationExample::setupStateManagerConnections()
{
    if (!m_stateManager) return;
    
    connect(m_stateManager, &AsyncStateManager::deviceStateChanged,
            this, &AsyncIntegrationExample::onStateDeviceStateChanged);
    connect(m_stateManager, &AsyncStateManager::petBehaviorChanged,
            this, &AsyncIntegrationExample::onStateBehaviorChanged);
    connect(m_stateManager, &AsyncStateManager::emotionChanged,
            this, &AsyncIntegrationExample::onStateEmotionChanged);
    
    logDebug("State manager connections established");
}

void AsyncIntegrationExample::setupUIUpdaterConnections()
{
    if (!m_uiUpdater) return;
    
    connect(m_uiUpdater, &ThreadSafeUIUpdater::updateProcessed,
            this, &AsyncIntegrationExample::onUIUpdateCompleted);
    
    logDebug("UI updater connections established");
}

void AsyncIntegrationExample::setupMessageQueueConnections()
{
    if (!m_messageQueue) return;
    
    // 消息队列信号连接
    connect(m_messageQueue, &AsyncMessageQueue::queueSizeChanged,
            [this](int size) {
                if (size > 500) {
                    logInfo(QString("Message queue size: %1").arg(size));
                }
            });
    
    connect(m_messageQueue, &AsyncMessageQueue::queueOverflow,
            [this]() {
                logError("Message queue overflow!");
            });
    
    logDebug("Message queue connections established");
}

// 工作线程回调
void AsyncIntegrationExample::onWorkerConnected()
{
    logInfo("Worker connected to server");
    m_connected = true;
    m_stateManager->setConnectionState(true);
    emit connected();
}

void AsyncIntegrationExample::onWorkerDisconnected()
{
    logInfo("Worker disconnected from server");
    m_connected = false;
    m_stateManager->setConnectionState(false);
    emit disconnected();
}

void AsyncIntegrationExample::onWorkerError(const QString& error)
{
    logError(QString("Worker error: %1").arg(error));
    m_uiUpdater->showErrorMessage(error);
    emit connectionError(error);
}

void AsyncIntegrationExample::onWorkerResponseReady(const QString& response)
{
    logDebug(QString("Response ready: %1").arg(response));
    emit messageReceived(response);
}

void AsyncIntegrationExample::onWorkerAudioReady(const QByteArray& audioData)
{
    logDebug(QString("Audio ready, size: %1 bytes").arg(audioData.size()));
    emit audioReceived(audioData);
}

void AsyncIntegrationExample::onWorkerEmotionChanged(const QString& emotion)
{
    logDebug(QString("Worker emotion changed: %1").arg(emotion));
    m_stateManager->setEmotion(emotion);
}

void AsyncIntegrationExample::onWorkerAnimationRequested(const QString& animation)
{
    logDebug(QString("Worker animation requested: %1").arg(animation));
    m_stateManager->setAnimation(animation);
}

// 状态管理器回调
void AsyncIntegrationExample::onStateDeviceStateChanged(const QString& oldState, const QString& newState)
{
    logInfo(QString("Device state changed: %1 -> %2").arg(oldState, newState));
    emit deviceStateChanged(newState);
}

void AsyncIntegrationExample::onStateBehaviorChanged(const QString& oldBehavior, const QString& newBehavior)
{
    logInfo(QString("Behavior changed: %1 -> %2").arg(oldBehavior, newBehavior));
    emit behaviorChanged(newBehavior);
}

void AsyncIntegrationExample::onStateEmotionChanged(const QString& oldEmotion, const QString& newEmotion)
{
    logInfo(QString("Emotion changed: %1 -> %2").arg(oldEmotion, newEmotion));
    emit emotionChanged(newEmotion);
}

// UI更新器回调
void AsyncIntegrationExample::onUIUpdateCompleted(const UIUpdateMessage& update)
{
    logDebug(QString("UI update completed, type: %1").arg(static_cast<int>(update.type)));
    emit uiUpdateCompleted();
}

// 日志方法
void AsyncIntegrationExample::logDebug(const QString& message)
{
    qDebug() << "[AsyncIntegration]" << message;
}

void AsyncIntegrationExample::logError(const QString& message)
{
    qCritical() << "[AsyncIntegration]" << message;
}

void AsyncIntegrationExample::logInfo(const QString& message)
{
    qInfo() << "[AsyncIntegration]" << message;
}
