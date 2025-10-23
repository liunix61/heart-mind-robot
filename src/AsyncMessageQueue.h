//
// 异步消息队列系统
// 用于将对话处理从UI线程分离，提升渲染性能
//

#ifndef ASYNCMESSAGEQUEUE_H
#define ASYNCMESSAGEQUEUE_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QTimer>
#include <QJsonObject>
#include <QByteArray>
#include <QString>

// 消息类型枚举
enum class AsyncMessageType {
    TEXT_MESSAGE,
    AUDIO_MESSAGE,
    STATE_UPDATE,
    EMOTION_UPDATE,
    ANIMATION_REQUEST,
    UI_UPDATE
};

// 消息结构体
struct AsyncMessage {
    AsyncMessageType type;
    QJsonObject data;
    QString text;
    QByteArray audioData;
    QString emotion;
    QString animation;
    qint64 timestamp;
    
    AsyncMessage() : type(AsyncMessageType::TEXT_MESSAGE), timestamp(0) {}
    AsyncMessage(AsyncMessageType t, const QJsonObject& d) 
        : type(t), data(d), timestamp(QDateTime::currentMSecsSinceEpoch()) {}
};

// UI更新消息
struct UIUpdate {
    enum UpdateType {
        CHAT_MESSAGE,
        PET_ANIMATION,
        PET_EMOTION,
        STATUS_UPDATE,
        AUDIO_PLAYBACK
    };
    
    UpdateType type;
    QString content;
    QJsonObject data;
    QByteArray audioData;
    
    UIUpdate(UpdateType t, const QString& c) : type(t), content(c) {}
    UIUpdate(UpdateType t, const QJsonObject& d) : type(t), data(d) {}
    UIUpdate(UpdateType t, const QByteArray& audio) : type(t), audioData(audio) {}
};

class AsyncMessageQueue : public QObject
{
    Q_OBJECT

public:
    explicit AsyncMessageQueue(QObject *parent = nullptr);
    ~AsyncMessageQueue();

    // 消息队列操作
    void enqueueMessage(const AsyncMessage& message);
    void enqueueTextMessage(const QString& text);
    void enqueueAudioMessage(const QByteArray& audioData);
    void enqueueStateUpdate(const QString& state);
    void enqueueEmotionUpdate(const QString& emotion);
    void enqueueAnimationRequest(const QString& animation);
    
    // UI更新调度
    void scheduleUIUpdate(const UIUpdate& update);
    
    // 队列状态
    bool hasMessages() const;
    int queueSize() const;
    void clearQueue();

signals:
    // 消息处理信号
    void messageReady(const AsyncMessage& message);
    void uiUpdateReady(const UIUpdate& update);
    
    // 队列状态信号
    void queueSizeChanged(int size);
    void queueOverflow();

private slots:
    void processMessageQueue();
    void processUIUpdates();

private:
    // 消息队列
    QQueue<AsyncMessage> m_messageQueue;
    QQueue<UIUpdate> m_uiUpdateQueue;
    
    // 线程安全
    mutable QMutex m_messageMutex;
    mutable QMutex m_uiMutex;
    
    // 处理定时器
    QTimer* m_messageTimer;
    QTimer* m_uiTimer;
    
    // 配置
    static const int MAX_QUEUE_SIZE = 1000;
    static const int MESSAGE_PROCESS_INTERVAL = 50; // 50ms
    static const int UI_UPDATE_INTERVAL = 16; // 16ms (60fps)
    
    // 内部方法
    void initializeTimers();
    void processSingleMessage(const AsyncMessage& message);
    void processSingleUIUpdate(const UIUpdate& update);
};

#endif // ASYNCMESSAGEQUEUE_H

