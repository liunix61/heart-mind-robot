//
// 异步组件集成示例
// 展示如何在DeskPetIntegration中使用异步组件
//

#ifndef ASYNCINTEGRATIONEXAMPLE_H
#define ASYNCINTEGRATIONEXAMPLE_H

#include <QObject>
#include "AsyncMessageQueue.h"
#include "ConversationWorker.h"
#include "ThreadSafeUIUpdater.h"
#include "AsyncStateManager.h"

// 前向声明
class WebSocketChatDialog;
class DeskPetIntegration;
class LAppLive2DManager;

/**
 * @brief 异步集成示例类
 * 
 * 这个类展示了如何将异步组件集成到现有的DeskPetIntegration中
 * 主要功能：
 * 1. 初始化所有异步组件
 * 2. 建立信号槽连接
 * 3. 提供异步消息处理接口
 * 4. 确保线程安全的UI更新
 */
class AsyncIntegrationExample : public QObject
{
    Q_OBJECT

public:
    explicit AsyncIntegrationExample(QObject *parent = nullptr);
    ~AsyncIntegrationExample();

    // 初始化
    bool initialize(WebSocketChatDialog* chatDialog, 
                   DeskPetIntegration* integration,
                   LAppLive2DManager* live2DManager);
    
    // 连接管理
    bool connectToServer(const QString& serverUrl, 
                        const QString& accessToken,
                        const QString& deviceId);
    void disconnectFromServer();
    bool isConnected() const;
    
    // 消息发送（异步，不阻塞UI）
    void sendTextMessage(const QString& text);
    void sendAudioMessage(const QByteArray& audioData);
    
    // 状态管理（异步）
    void updatePetBehavior(const QString& behavior);
    void updateEmotion(const QString& emotion);
    void updateAnimation(const QString& animation);
    void updateDeviceState(const QString& state);
    
    // 状态查询
    QString getCurrentBehavior() const;
    QString getCurrentEmotion() const;
    QString getCurrentDeviceState() const;
    StateSnapshot getStateSnapshot() const;
    
    // 关闭
    void shutdown();

signals:
    // 连接状态信号
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    
    // 消息接收信号
    void messageReceived(const QString& message);
    void audioReceived(const QByteArray& audioData);
    
    // 状态变化信号
    void behaviorChanged(const QString& newBehavior);
    void emotionChanged(const QString& newEmotion);
    void deviceStateChanged(const QString& newState);
    
    // UI更新完成信号
    void uiUpdateCompleted();

private slots:
    // 工作线程回调
    void onWorkerConnected();
    void onWorkerDisconnected();
    void onWorkerError(const QString& error);
    void onWorkerResponseReady(const QString& response);
    void onWorkerAudioReady(const QByteArray& audioData);
    void onWorkerEmotionChanged(const QString& emotion);
    void onWorkerAnimationRequested(const QString& animation);
    
    // 状态管理器回调
    void onStateDeviceStateChanged(const QString& oldState, const QString& newState);
    void onStateBehaviorChanged(const QString& oldBehavior, const QString& newBehavior);
    void onStateEmotionChanged(const QString& oldEmotion, const QString& newEmotion);
    
    // UI更新器回调
    void onUIUpdateCompleted(const UIUpdateMessage& update);

private:
    // 异步组件
    AsyncMessageQueue* m_messageQueue;
    ConversationWorker* m_conversationWorker;
    ThreadSafeUIUpdater* m_uiUpdater;
    AsyncStateManager* m_stateManager;
    
    // UI组件引用（不拥有所有权）
    WebSocketChatDialog* m_chatDialog;
    DeskPetIntegration* m_integration;
    LAppLive2DManager* m_live2DManager;
    
    // 状态
    bool m_initialized;
    bool m_connected;
    
    // 内部方法
    void setupConnections();
    void setupWorkerConnections();
    void setupStateManagerConnections();
    void setupUIUpdaterConnections();
    void setupMessageQueueConnections();
    
    // 辅助方法
    void logDebug(const QString& message);
    void logError(const QString& message);
    void logInfo(const QString& message);
};

#endif // ASYNCINTEGRATIONEXAMPLE_H
