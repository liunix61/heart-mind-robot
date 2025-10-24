//
// ConversationWorker
// Handle WebSocket communication and conversation logic, avoid blocking UI thread
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

// Forward declarations
class AsyncMessageQueue;
class DeskPetIntegration;

// Worker thread state
enum class WorkerState {
    IDLE,
    CONNECTING,
    CONNECTED,
    LISTENING,
    PROCESSING,
    SPEAKING,
    ERROR
};

// Message processing result
struct ProcessingResult {
    bool success;
    QString errorMessage;
    QJsonObject response;
    QByteArray audioData;
};

// Conversation worker thread
class ConversationWorker : public QThread
{
    Q_OBJECT

public:
    explicit ConversationWorker(QObject *parent = nullptr);
    ~ConversationWorker();

    // Start/stop worker
    void startWorker();
    void stopWorker();

    // Send message to worker
    void sendMessage(const QJsonObject &message);

    // Get current state
    WorkerState getCurrentState() const;

signals:
    // State change signals
    void stateChanged(WorkerState newState);
    void connectionStatusChanged(bool connected);
    
    // Message processing signals
    void messageProcessed(const ProcessingResult &result);
    void audioDataReceived(const QByteArray &audioData);
    
    // Error signals
    void errorOccurred(const QString &errorMessage);

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketTextMessageReceived(const QString &message);
    void onWebSocketBinaryMessageReceived(const QByteArray &message);
    void onTimeout();

protected:
    void run() override;

private:
    // WebSocket connection
    QWebSocket *m_webSocket;
    QString m_webSocketUrl;
    bool m_isConnected;
    
    // Message queue
    QQueue<QJsonObject> m_messageQueue;
    QMutex m_queueMutex;
    
    // State management
    WorkerState m_currentState;
    QMutex m_stateMutex;
    
    // Timer for timeouts
    QTimer *m_timeoutTimer;
    
    // Integration with other components
    AsyncMessageQueue *m_messageQueue;
    DeskPetIntegration *m_deskPetIntegration;
    
    // Helper methods
    void setState(WorkerState newState);
    void processMessage(const QJsonObject &message);
    void sendWebSocketMessage(const QJsonObject &message);
    void handleAudioData(const QByteArray &audioData);
};

#endif // CONVERSATIONWORKER_H
