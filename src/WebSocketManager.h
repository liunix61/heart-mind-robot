#ifndef WEBSOCKETMANAGER_H
#define WEBSOCKETMANAGER_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDebug>
#include <QThread>
#include <QMutex>
#include <QQueue>

// 设备状态枚举
enum class DeviceState {
    IDLE,
    LISTENING,
    SPEAKING,
    CONNECTING,
    DISCONNECTED
};

// 消息类型枚举
enum class MessageType {
    HELLO,
    LISTEN,
    ABORT,
    TTS,
    STT,
    LLM,
    IOT,
    MCP,      // Model Context Protocol
    PING,
    PONG
};

// 消息结构体
struct WebSocketMessage {
    MessageType type;
    QJsonObject data;
    QString sessionId;
    QString timestamp;
};

class WebSocketManager : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketManager(QObject *parent = nullptr);
    ~WebSocketManager();

    // 连接管理
    bool connectToServer(const QString &url, const QString &accessToken);
    void disconnectFromServer();
    bool isConnected() const;
    
    // 消息发送
    void sendHello();
    void sendListenStart();
    void sendListenStop();
    void sendAbortSpeaking();
    void sendWakeWordDetected(const QString &text);
    void sendAudioData(const QByteArray &audioData);
    
    // 状态管理
    DeviceState getCurrentState() const;
    void setCurrentState(DeviceState state);
    
    // 配置管理
    void setDeviceId(const QString &deviceId);
    void setClientId(const QString &clientId);
    void setAccessToken(const QString &token);
    
    // 心跳管理
    void startHeartbeat();
    void stopHeartbeat();

signals:
    // 连接状态信号
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    
    // 消息接收信号
    void messageReceived(const WebSocketMessage &message);
    void ttsMessageReceived(const QString &text, const QString &emotion);
    void sttMessageReceived(const QString &text);
    void llmMessageReceived(const QString &text, const QString &emotion);
    void iotCommandReceived(const QJsonObject &command);
    
    // 状态变化信号
    void stateChanged(DeviceState newState);
    
    // 音频信号
    void audioDataReceived(const QByteArray &audioData);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &data);
    void onError(QAbstractSocket::SocketError error);
    void onHeartbeatTimeout();
    void onPongReceived(quint64 elapsedTime, const QByteArray &payload);
    void onReconnectTimeout();

private:
    // 连接管理
    QWebSocket *m_webSocket;
    QUrl m_serverUrl;
    QString m_accessToken;
    QString m_deviceId;
    QString m_clientId;
    QString m_sessionId;
    bool m_connected;
    
    // 心跳管理
    QTimer *m_heartbeatTimer;
    QTimer *m_pongTimer;
    bool m_pongReceived;
    int m_heartbeatInterval;
    int m_pongTimeout;
    
    // 重连管理
    QTimer *m_reconnectTimer;
    int m_reconnectInterval;
    int m_reconnectAttempts;
    int m_maxReconnectAttempts;
    
    // 消息处理
    QQueue<WebSocketMessage> m_messageQueue;
    QMutex m_messageMutex;
    
    // 状态管理
    DeviceState m_currentState;
    mutable QMutex m_stateMutex;
    
    // 配置
    QString m_protocolVersion;
    
    // 内部方法
    void initializeWebSocket();
    void processIncomingMessage(const QString &message);
    void processIncomingBinary(const QByteArray &data);
    WebSocketMessage parseMessage(const QJsonObject &json);
    void sendMessage(const WebSocketMessage &message);
    void handleHelloResponse(const QJsonObject &data);
    void handleTTSMessage(const QJsonObject &data);
    void handleSTTMessage(const QJsonObject &data);
    void handleLLMMessage(const QJsonObject &data);
    void handleIoTMessage(const QJsonObject &data);
    void handlePingMessage(const QJsonObject &data);
    void handlePongMessage(const QJsonObject &data);
    void handleMCPMessage(const QJsonObject &data);
    void attemptReconnect();
    void startReconnect();
    void stopReconnect();
    
    // 工具方法
    QString generateSessionId();
    QString getCurrentTimestamp();
    QJsonObject createBaseMessage(MessageType type);
};

#endif // WEBSOCKETMANAGER_H
