#ifndef DESKPETINTEGRATION_H
#define DESKPETINTEGRATION_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QString>
#include <QByteArray>
#include "DeskPetController.h"
#include "mainwindow.h"
#include "LAppLive2DManager.hpp"
#include "AudioUtil.h"

// 桌宠集成类 - 将WebSocket客户端集成到现有的Live2D桌宠系统中
class DeskPetIntegration : public QObject
{
    Q_OBJECT

public:
    explicit DeskPetIntegration(QObject *parent = nullptr);
    ~DeskPetIntegration();

    // 初始化和配置
    bool initialize(MainWindow *mainWindow);
    void shutdown();
    
    // 连接管理
    bool connectToServer();
    void disconnectFromServer();
    bool isConnected() const;
    
    // 桌宠控制
    void startListening();
    void stopListening();
    void sendTextMessage(const QString &text);
    void sendVoiceMessage(const QByteArray &audioData);
    void sendAudioData(const QByteArray &audioData);  // 发送音频流数据
    void abortSpeaking();
    
    // 状态查询
    PetBehavior getCurrentBehavior() const;
    DeviceState getCurrentDeviceState() const;
    bool isListening() const;
    bool isSpeaking() const;
    
    // 配置管理
    void setServerUrl(const QString &url);
    void setAccessToken(const QString &token);
    
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
    
    // 音频播放
    void playAudioData(const QByteArray &audioData);

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
    
    // 桌宠交互信号
    void petInteraction(const QString &interaction);
    void animationRequested(const QString &animationName);
    
    // 调试信号
    void debugMessage(const QString &message);

private slots:
    // DeskPetController信号处理
    void onControllerConnected();
    void onControllerDisconnected();
    void onControllerError(const QString &error);
    void onControllerBehaviorChanged(PetBehavior newBehavior);
    void onControllerDeviceStateChanged(DeviceState newState);
    void onControllerMessageReceived(const QString &message);
    void onControllerAudioReceived(const QByteArray &audioData);
    void onControllerEmotionChanged(const QString &emotion);
    void onControllerPetInteraction(const QString &interaction);
    void onControllerAnimationRequested(const QString &animationName);
    void onControllerDebugMessage(const QString &message);
    void onControllerSTTReceived(const QString &text);
    
    // 定时器处理
    void onStatusUpdateTimeout();
    void onHeartbeatTimeout();

private slots:
    // 音频解码完成处理
    void onAudioDecoded(const QByteArray &pcmData);

private:
    // 音频流累积缓冲
    QByteArray m_accumulatedPcmData;
    QTimer *m_lipSyncTimer;
    
    // 待发送消息队列（用于重连后自动发送）
    QString m_pendingTextMessage;
    bool m_hasPendingMessage;
    
    // 核心组件
    DeskPetController *m_controller;
    MainWindow *m_mainWindow;
    LAppLive2DManager *m_live2DManager;
    AudioPlayer *m_audioPlayer;
    
    // 定时器
    QTimer *m_statusUpdateTimer;
    QTimer *m_heartbeatTimer;
    
    // 状态变量
    bool m_initialized;
    bool m_connected;
    bool m_lipSyncEnabled;  // 控制是否启用口型同步（播放音乐时禁用）
    
    // 配置
    QString m_serverUrl;
    QString m_accessToken;
    QString m_deviceId;
    QString m_clientId;
    
    // 内部方法
    void setupConnections();
    void setupTimers();
    void loadConfiguration();
    void saveConfiguration();
    void updateLive2DState();
    void handleBehaviorChange(PetBehavior behavior);
    void handleEmotionChange(const QString &emotion);
    void handleAnimationRequest(const QString &animationName);
    
    // 音频处理辅助方法
    QByteArray convertPCMToWAV(const QByteArray &pcmData, int sampleRate = 24000, int channels = 1, int bitsPerSample = 16);
    
    // 调试和日志
    void logDebug(const QString &message);
    void logError(const QString &message);
    void logInfo(const QString &message);
};

#endif // DESKPETINTEGRATION_H
