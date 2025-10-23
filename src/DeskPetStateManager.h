#ifndef DESKPETSTATEMANAGER_H
#define DESKPETSTATEMANAGER_H

#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QMutex>
#include <QJsonObject>
#include <QString>
#include <QByteArray>
#include "WebSocketManager.h"

// 桌宠行为状态
enum class PetBehavior {
    IDLE,
    LISTENING,
    SPEAKING,
    THINKING,
    EXCITED,
    SAD,
    ANGRY,
    SLEEPING
};

// 动画类型
enum class AnimationType {
    IDLE_LOOP,
    LISTENING,
    SPEAKING,
    THINKING,
    EXCITED,
    SAD,
    ANGRY,
    SLEEPING,
    WAKE_UP,
    GREETING
};

// 音频状态
enum class AudioState {
    SILENT,
    RECORDING,
    PLAYING,
    PROCESSING
};

class DeskPetStateManager : public QObject
{
    Q_OBJECT

public:
    explicit DeskPetStateManager(QObject *parent = nullptr);
    ~DeskPetStateManager();

    // 状态管理
    PetBehavior getCurrentBehavior() const;
    AudioState getCurrentAudioState() const;
    DeviceState getCurrentDeviceState() const;
    
    // 行为控制
    void setBehavior(PetBehavior behavior);
    void setAudioState(AudioState state);
    void setDeviceState(DeviceState state);
    
    // 动画控制
    void playAnimation(AnimationType type);
    void stopCurrentAnimation();
    AnimationType getCurrentAnimation() const;
    
    // 消息处理
    void processIncomingMessage(const WebSocketMessage &message);
    void processTTSMessage(const QString &text, const QString &emotion);
    void processSTTMessage(const QString &text);
    void processLLMMessage(const QString &text, const QString &emotion);
    void processIoTCommand(const QJsonObject &command);
    
    // 音频处理
    void startRecording();
    void stopRecording();
    void startPlaying();
    void stopPlaying();
    
    // 状态查询
    bool isIdle() const;
    bool isListening() const;
    bool isSpeaking() const;
    bool isProcessing() const;
    
    // 配置管理
    void setAutoResponse(bool enabled);
    void setVoiceEnabled(bool enabled);
    void setAnimationEnabled(bool enabled);
    
    // 消息队列管理
    void queueMessage(const QString &message);
    QString dequeueMessage();
    bool hasQueuedMessages() const;
    int getQueueSize() const;

signals:
    // 状态变化信号
    void behaviorChanged(PetBehavior newBehavior);
    void audioStateChanged(AudioState newState);
    void deviceStateChanged(DeviceState newState);
    
    // 动画控制信号
    void animationRequested(AnimationType type);
    void animationStopped();
    
    // 音频控制信号
    void startRecordingRequested();
    void stopRecordingRequested();
    void startPlayingRequested();
    void stopPlayingRequested();
    
    // 消息处理信号
    void messageToSend(const QString &message);
    void audioDataToSend(const QByteArray &data);
    
    // 桌宠交互信号
    void petInteraction(const QString &interaction);
    void emotionChanged(const QString &emotion);

private slots:
    void onBehaviorTimeout();
    void onAudioTimeout();
    void onProcessingTimeout();
    void processMessageQueue();

private:
    // 状态变量
    PetBehavior m_currentBehavior;
    AudioState m_currentAudioState;
    DeviceState m_currentDeviceState;
    AnimationType m_currentAnimation;
    
    // 定时器
    QTimer *m_behaviorTimer;
    QTimer *m_audioTimer;
    QTimer *m_processingTimer;
    QTimer *m_messageQueueTimer;
    
    // 消息队列
    QQueue<QString> m_messageQueue;
    mutable QMutex m_queueMutex;
    
    // 配置
    bool m_autoResponse;
    bool m_voiceEnabled;
    bool m_animationEnabled;
    
    // 状态持续时间
    int m_idleTimeout;
    int m_listeningTimeout;
    int m_speakingTimeout;
    int m_processingTimeout;
    
    // 内部方法
    void initializeTimers();
    void updateBehaviorBasedOnDeviceState();
    void updateAnimationBasedOnBehavior();
    void handleEmotionChange(const QString &emotion);
    void processTextForEmotion(const QString &text);
    QString extractEmotionFromText(const QString &text);
    AnimationType getAnimationForBehavior(PetBehavior behavior);
    AnimationType getAnimationForEmotion(const QString &emotion);
    
    // 状态转换逻辑
    void transitionToIdle();
    void transitionToListening();
    void transitionToSpeaking();
    void transitionToThinking();
    void transitionToExcited();
    void transitionToSad();
    void transitionToAngry();
    void transitionToSleeping();
};

#endif // DESKPETSTATEMANAGER_H
