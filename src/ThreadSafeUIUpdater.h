//
// 线程安全的UI更新器
// 确保UI更新在UI线程中执行，避免线程安全问题
//

#ifndef THREADSAFEUIUPDATER_H
#define THREADSAFEUIUPDATER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QByteArray>
#include <QMutex>
#include <QQueue>
#include <QTimer>

// 前向声明
class WebSocketChatDialog;
class DeskPetIntegration;
class LAppLive2DManager;

// UI更新类型
enum class UIUpdateType {
    CHAT_MESSAGE,
    PET_ANIMATION,
    PET_EMOTION,
    STATUS_UPDATE,
    AUDIO_PLAYBACK,
    CONNECTION_STATUS,
    ERROR_MESSAGE
};

// UI更新消息
struct UIUpdateMessage {
    UIUpdateType type;
    QString content;
    QJsonObject data;
    QByteArray audioData;
    QString animation;
    QString emotion;
    QString status;
    bool isError;
    
    UIUpdateMessage() : isError(false) {}
    UIUpdateMessage(UIUpdateType t, const QString& c) 
        : type(t), content(c), isError(false) {}
    UIUpdateMessage(UIUpdateType t, const QJsonObject& d) 
        : type(t), data(d), isError(false) {}
    UIUpdateMessage(UIUpdateType t, const QByteArray& audio) 
        : type(t), audioData(audio), isError(false) {}
};

class ThreadSafeUIUpdater : public QObject
{
    Q_OBJECT

public:
    explicit ThreadSafeUIUpdater(QObject *parent = nullptr);
    ~ThreadSafeUIUpdater();

    // 设置UI组件
    void setChatDialog(WebSocketChatDialog* dialog);
    void setDeskPetIntegration(DeskPetIntegration* integration);
    void setLive2DManager(LAppLive2DManager* manager);
    
    // 线程安全的UI更新方法
    void updateChatMessage(const QString& message);
    void updatePetAnimation(const QString& animation);
    void updatePetEmotion(const QString& emotion);
    void updateStatus(const QString& status);
    void playAudioSafely(const QByteArray& audioData);
    void updateConnectionStatus(bool connected);
    void showErrorMessage(const QString& error);
    
    // 批量更新
    void scheduleUIUpdate(const UIUpdateMessage& update);
    void processPendingUpdates();
    
    // 状态查询
    bool hasPendingUpdates() const;
    int pendingUpdateCount() const;
    void clearPendingUpdates();

signals:
    // UI更新信号
    void chatMessageUpdated(const QString& message);
    void petAnimationUpdated(const QString& animation);
    void petEmotionUpdated(const QString& emotion);
    void statusUpdated(const QString& status);
    void audioPlaybackRequested(const QByteArray& audioData);
    void connectionStatusUpdated(bool connected);
    void errorMessageShown(const QString& error);
    
    // 内部信号
    void updateProcessed(const UIUpdateMessage& update);

private slots:
    void processUpdateQueue();
    void handleUpdateProcessed(const UIUpdateMessage& update);

private:
    // UI组件引用
    WebSocketChatDialog* m_chatDialog;
    DeskPetIntegration* m_deskPetIntegration;
    LAppLive2DManager* m_live2DManager;
    
    // 更新队列
    QQueue<UIUpdateMessage> m_updateQueue;
    mutable QMutex m_queueMutex;
    
    // 处理定时器
    QTimer* m_processTimer;
    
    // 配置
    static const int UPDATE_PROCESS_INTERVAL = 16; // 16ms (60fps)
    static const int MAX_QUEUE_SIZE = 100;
    
    // 内部方法
    void initializeTimer();
    void processSingleUpdate(const UIUpdateMessage& update);
    void updateChatDialog(const QString& message);
    void updatePetAnimationInternal(const QString& animation);
    void updatePetEmotionInternal(const QString& emotion);
    void updateStatusInternal(const QString& status);
    void playAudioInternal(const QByteArray& audioData);
    void updateConnectionStatusInternal(bool connected);
    void showErrorInternal(const QString& error);
    
    // 安全检查
    bool isUIThread() const;
    void ensureUIThread();
};

#endif // THREADSAFEUIUPDATER_H

