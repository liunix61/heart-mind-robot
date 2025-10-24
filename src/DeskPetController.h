#ifndef DESKPETCONTROLLER_H
#define DESKPETCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QAudioInput>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QBuffer>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "WebSocketManager.h"
#include "DeskPetStateManager.h"
#include "ConfigManager.h"
#include "LAppLive2DManager.hpp"
#include "LAppModel.hpp"

// 音频配置
struct AudioConfig {
    int sampleRate = 16000;
    int channels = 1;
    int sampleSize = 16;
    int frameSize = 320; // 20ms at 16kHz
    QString codec = "opus";
};

// 桌宠控制器 - 整合WebSocket、状态管理和Live2D
class DeskPetController : public QObject
{
    Q_OBJECT

public:
    explicit DeskPetController(QObject *parent = nullptr);
    ~DeskPetController();

    // 初始化和配置
    bool initialize();
    void shutdown();
    
    // 连接管理
    bool connectToServer();
    void disconnectFromServer();
    bool isConnected() const;
    
    // 桌宠控制
    void startListening();
    void stopListening();
    void sendTextMessage(const QString &text);
    void sendAudioMessage(const QByteArray &audioData);
    void abortSpeaking();
    
    // 状态查询
    PetBehavior getCurrentBehavior() const;
    DeviceState getCurrentDeviceState() const;
    bool isListening() const;
    bool isSpeaking() const;
    
    // 配置管理
    void setServerUrl(const QString &url);
    void setAccessToken(const QString &token);
    void setDeviceId(const QString &deviceId);
    void setClientId(const QString &clientId);
    
    // 音频控制
    void setAudioEnabled(bool enabled);
    void setMicrophoneEnabled(bool enabled);
    void setSpeakerEnabled(bool enabled);
    
    // 动画控制
    void setAnimationEnabled(bool enabled);
    void playAnimation(const QString &animationName);
    void stopCurrentAnimation();
    
    // 消息处理
    void processUserInput(const QString &input);
    void processVoiceInput(const QByteArray &audioData);

signals:
    // 连接状态信号
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    
    // 状态变化信号
    void behaviorChanged(PetBehavior newBehavior);
    void deviceStateChanged(DeviceState newState);
    
    // 消息信号
    void messageReceived(const QString &message);
    void audioReceived(const QByteArray &audioData);
    void emotionChanged(const QString &emotion);
    void sttReceived(const QString &text);  // 用户语音识别消息
    
    // 桌宠交互信号
    void petInteraction(const QString &interaction);
    void animationRequested(const QString &animationName);
    
    // 调试信号
    void debugMessage(const QString &message);

private slots:
    // WebSocket信号处理
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(const QString &error);
    void onWebSocketMessageReceived(const WebSocketMessage &message);
    void onWebSocketTTSReceived(const QString &text, const QString &emotion);
    void onWebSocketSTTReceived(const QString &text);
    void onWebSocketLLMReceived(const QString &text, const QString &emotion);
    void onWebSocketIoTReceived(const QJsonObject &command);
    void onWebSocketAudioReceived(const QByteArray &audioData);
    
    // 状态管理信号处理
    void onBehaviorChanged(PetBehavior newBehavior);
    void onAudioStateChanged(AudioState newState);
    void onDeviceStateChanged(DeviceState newState);
    void onAnimationRequested(AnimationType type);
    void onAnimationStopped();
    void onStartRecordingRequested();
    void onStopRecordingRequested();
    void onStartPlayingRequested();
    void onStopPlayingRequested();
    void onMessageToSend(const QString &message);
    void onAudioDataToSend(const QByteArray &data);
    void onPetInteraction(const QString &interaction);
    void onEmotionChanged(const QString &emotion);
    
    // 音频处理
    void onAudioInputData();
    void onAudioOutputData();
    void onAudioInputError();
    void onAudioOutputError();
    
    // 定时器处理
    void onHeartbeatTimeout();
    void onStatusUpdateTimeout();

private:
    // 核心组件
    WebSocketManager *m_webSocketManager;
    DeskPetStateManager *m_stateManager;
    ConfigManager *m_configManager;
    
    // Live2D集成
    LAppLive2DManager *m_live2DManager;
    
    // 音频组件
    QBuffer *m_audioBuffer;
    AudioConfig m_audioConfig;
    
    // 网络组件
    QNetworkAccessManager *m_networkManager;
    
    // 定时器
    QTimer *m_heartbeatTimer;
    QTimer *m_statusUpdateTimer;
    
    // 状态变量
    bool m_initialized;
    bool m_audioEnabled;
    bool m_microphoneEnabled;
    bool m_speakerEnabled;
    bool m_animationEnabled;
    
    // 配置
    QString m_serverUrl;
    QString m_accessToken;
    QString m_deviceId;
    QString m_clientId;
    
    // 内部方法
    void initializeComponents();
    void setupConnections();
    void setupAudio();
    void setupTimers();
    void loadConfiguration();
    void saveConfiguration();
    
    // 音频处理
    void startAudioInput();
    void stopAudioInput();
    void startAudioOutput();
    void stopAudioOutput();
    QByteArray processAudioData(const QByteArray &inputData);
    void playAudioData(const QByteArray &audioData);
    
    // 动画处理
    void handleAnimationRequest(AnimationType type);
    QString getAnimationName(AnimationType type);
    void updateLive2DAnimation(const QString &animationName);
    
    // 消息处理
    void handleIncomingMessage(const WebSocketMessage &message);
    void handleTTSMessage(const QString &text, const QString &emotion);
    void handleSTTMessage(const QString &text);
    void handleLLMMessage(const QString &text, const QString &emotion);
    void handleIoTCommand(const QJsonObject &command);
    
    // 状态同步
    void syncStateWithLive2D();
    void updateLive2DState();
    
    // 调试和日志
    void logDebug(const QString &message);
    void logError(const QString &message);
    void logInfo(const QString &message);
};

#endif // DESKPETCONTROLLER_H
