//
// 异步状态管理器
// 参考py-xiaozhi的状态管理，实现异步状态更新
//

#ifndef ASYNCSTATEMANAGER_H
#define ASYNCSTATEMANAGER_H

#include <QObject>
#include <QString>
#include <QMutex>
#include <QQueue>
#include <QTimer>
#include <QJsonObject>

// 前向声明
class ThreadSafeUIUpdater;

// 状态类型
enum class StateType {
    DEVICE_STATE,
    PET_BEHAVIOR,
    EMOTION_STATE,
    ANIMATION_STATE,
    AUDIO_STATE,
    CONNECTION_STATE
};

// 状态变化消息
struct StateChange {
    StateType type;
    QString oldValue;
    QString newValue;
    QJsonObject data;
    qint64 timestamp;
    bool isError;
    
    StateChange() : isError(false), timestamp(0) {}
    StateChange(StateType t, const QString& oldVal, const QString& newVal)
        : type(t), oldValue(oldVal), newValue(newVal), isError(false)
        , timestamp(QDateTime::currentMSecsSinceEpoch()) {}
};

// 状态快照
struct StateSnapshot {
    QString deviceState;
    QString petBehavior;
    QString emotion;
    QString animation;
    QString audioState;
    bool connected;
    qint64 timestamp;
    
    StateSnapshot() : connected(false), timestamp(0) {}
};

class AsyncStateManager : public QObject
{
    Q_OBJECT

public:
    explicit AsyncStateManager(QObject *parent = nullptr);
    ~AsyncStateManager();

    // 状态管理
    void setDeviceState(const QString& state);
    void setPetBehavior(const QString& behavior);
    void setEmotion(const QString& emotion);
    void setAnimation(const QString& animation);
    void setAudioState(const QString& state);
    void setConnectionState(bool connected);
    
    // 状态查询
    QString getDeviceState() const;
    QString getPetBehavior() const;
    QString getEmotion() const;
    QString getAnimation() const;
    QString getAudioState() const;
    bool isConnected() const;
    StateSnapshot getStateSnapshot() const;
    
    // 状态变化处理
    void processStateChange(const StateChange& change);
    void processPendingChanges();
    
    // 状态验证
    bool isValidState(const QString& state, StateType type) const;
    bool canTransitionTo(const QString& newState, StateType type) const;
    
    // 批量操作
    void batchUpdate(const QJsonObject& updates);
    void clearStateHistory();
    
    // 状态监听
    void addStateListener(StateType type, QObject* listener, const char* slot);
    void removeStateListener(StateType type, QObject* listener);

signals:
    // 状态变化信号
    void deviceStateChanged(const QString& oldState, const QString& newState);
    void petBehaviorChanged(const QString& oldBehavior, const QString& newBehavior);
    void emotionChanged(const QString& oldEmotion, const QString& newEmotion);
    void animationChanged(const QString& oldAnimation, const QString& newAnimation);
    void audioStateChanged(const QString& oldState, const QString& newState);
    void connectionStateChanged(bool oldConnected, bool newConnected);
    
    // 状态错误信号
    void stateError(const QString& error);
    void transitionError(const QString& from, const QString& to, const QString& error);
    
    // 状态快照信号
    void stateSnapshotUpdated(const StateSnapshot& snapshot);

private slots:
    void processStateQueue();
    void onStateChangeProcessed(const StateChange& change);

private:
    // 当前状态
    QString m_deviceState;
    QString m_petBehavior;
    QString m_emotion;
    QString m_animation;
    QString m_audioState;
    bool m_connected;
    
    // 状态历史
    QQueue<StateChange> m_stateHistory;
    QQueue<StateChange> m_pendingChanges;
    
    // 线程安全
    mutable QMutex m_stateMutex;
    mutable QMutex m_queueMutex;
    
    // 处理定时器
    QTimer* m_processTimer;
    
    // UI更新器
    ThreadSafeUIUpdater* m_uiUpdater;
    
    // 配置
    static const int MAX_HISTORY_SIZE = 100;
    static const int PROCESS_INTERVAL = 50; // 50ms
    
    // 内部方法
    void initializeTimer();
    void processSingleChange(const StateChange& change);
    void updateUIState(const StateChange& change);
    void validateStateChange(const StateChange& change);
    void notifyStateChange(const StateChange& change);
    void updateStateSnapshot();
    
    // 状态转换规则
    bool canTransitionDeviceState(const QString& from, const QString& to) const;
    bool canTransitionPetBehavior(const QString& from, const QString& to) const;
    bool canTransitionEmotion(const QString& from, const QString& to) const;
    bool canTransitionAnimation(const QString& from, const QString& to) const;
    bool canTransitionAudioState(const QString& from, const QString& to) const;
};

#endif // ASYNCSTATEMANAGER_H

