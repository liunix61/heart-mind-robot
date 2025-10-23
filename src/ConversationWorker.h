//
// 对话工作线程
// 处理WebSocket通信和对话逻辑，避免阻塞UI线程
//

#ifndef CONVERSATIONWORKER_H
#define CONVERSATIONWORKER_H

#include <QThread>
#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QJsonObject>
#include <QByteArray>
#include <QString>
#include <QTimer>
#include <QWebSocket>
#include <QJsonDocument>

// 前向声明
class AsyncMessageQueue;
class DeskPetIntegration;

// 工作线程状态
enum class WorkerState {
    IDLE,
    CONNECTING,
    CONNECTED,
    LISTENING,
    PROCESSING,
    SPEAKING,
    ERROR
};

// 消息处理结果
struct ProcessingResult {
    bool success;
    QString errorMessage;
    QJsonObject response;
    QByteArray audioData;
    QString emotion;
    QString animation;
    
    ProcessingResult() : success(false) {}
};

class ConversationWorker : public QThread
{
    Q_OBJECT

public:
    explicit ConversationWorker(QObject *parent = nullptr);
    ~ConversationWorker();

    // 线程控制
    void startWorker();
    void stopWorker();
    bool isRunning() const;
    
    // 消息处理
    void processTextMessage(const QString& text);
    void processAudioMessage(const QByteArray& audioData);
    void processWebSocketMessage(const QJsonObject& message);
    
    // 状态管理
    WorkerState getCurrentState() const;
    void setState(WorkerState state);
    
    // 配置
    void setServerUrl(const QString& url);
    void setAccessToken(const QString& token);
    void setDeviceId(const QString& deviceId);
    
    // 连接管理
    void connectToServer();
    void disconnectFromServer();
    bool isConnected() const;

signals:
    // 状态变化信号
    void stateChanged(WorkerState newState);
    void connected();
    void disconnected();
    void connectionError(const QString& error);
    
    // 消息处理信号
    void textMessageProcessed(const QString& text);
    void audioMessageProcessed(const QByteArray& audioData);
    void responseReady(const QString& response);
    void audioResponseReady(const QByteArray& audioData);
    
    // UI更新信号
    void uiUpdateRequested(const QString& updateType, const QJsonObject& data);
    void petAnimationRequested(const QString& animation);
    void petEmotionChanged(const QString& emotion);
    void statusUpdateRequested(const QString& status);
    
    // 错误处理信号
    void errorOccurred(const QString& error);
    void processingError(const QString& error);

protected:
    void run() override;

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketTextMessageReceived(const QString& message);
    void onWebSocketBinaryMessageReceived(const QByteArray& message);
    void onProcessingTimeout();
    void onHeartbeatTimeout();

private:
    // 线程状态
    volatile bool m_running;
    volatile bool m_shouldStop;
    WorkerState m_currentState;
    mutable QMutex m_stateMutex;
    
    // WebSocket连接
    QWebSocket* m_webSocket;
    QString m_serverUrl;
    QString m_accessToken;
    QString m_deviceId;
    QString m_clientId;
    bool m_connected;
    
    // 消息队列
    QQueue<QJsonObject> m_incomingMessages;
    QQueue<QString> m_outgoingMessages;
    QMutex m_messageMutex;
    
    // 定时器
    QTimer* m_processingTimer;
    QTimer* m_heartbeatTimer;
    
    // 配置
    static const int PROCESSING_TIMEOUT = 30000; // 30秒
    static const int HEARTBEAT_INTERVAL = 30000; // 30秒
    static const int MAX_QUEUE_SIZE = 100;
    
    // 内部方法
    void initializeTimers();
    void processIncomingMessages();
    void processOutgoingMessages();
    void handleWebSocketMessage(const QJsonObject& message);
    void handleTextMessage(const QString& text);
    void handleAudioMessage(const QByteArray& audioData);
    void handleStateUpdate(const QString& state);
    void handleEmotionUpdate(const QString& emotion);
    void handleAnimationRequest(const QString& animation);
    
    // 消息发送
    void sendTextMessage(const QString& text);
    void sendAudioMessage(const QByteArray& audioData);
    void sendHeartbeat();
    void sendConnectionRequest();
    
    // 错误处理
    void handleError(const QString& error);
    void handleProcessingError(const QString& error);
    
    // 状态更新
    void updateState(WorkerState newState);
    void notifyStateChange(WorkerState newState);
};

#endif // CONVERSATIONWORKER_H

