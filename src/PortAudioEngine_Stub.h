#ifndef PORTAUDIOENGINE_STUB_H
#define PORTAUDIOENGINE_STUB_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <QThread>

/**
 * @brief PortAudio引擎的存根实现（当PortAudio不可用时）
 * 
 * 这个类提供与PortAudioEngine相同的接口，但内部使用存根实现
 * 主要用于在没有安装PortAudio时保持代码的兼容性
 */
class PortAudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit PortAudioEngine(QObject *parent = nullptr);
    ~PortAudioEngine();

    // 初始化音频引擎
    bool initialize(int sampleRate = 24000, int channels = 1);
    
    // 开始/停止播放
    bool startPlayback();
    void stopPlayback();
    
    // 音频数据管理
    void enqueueAudio(const QByteArray &audioData);
    void clearQueue();
    
    // 设备管理
    struct AudioDevice {
        int deviceId;
        QString name;
        int maxInputChannels;
        int maxOutputChannels;
        double defaultSampleRate;
        bool isWASAPI;
    };
    
    static QList<AudioDevice> enumerateDevices();
    bool setOutputDevice(int deviceId);
    
    // 状态查询
    bool isInitialized() const { return m_initialized; }
    bool isPlaying() const { return m_isPlaying; }
    int getQueueSize() const;
    
signals:
    void playbackStarted();
    void playbackStopped();
    void errorOccurred(const QString &error);

private:
    // 成员变量
    bool m_initialized;
    bool m_isPlaying;
    int m_sampleRate;
    int m_channels;
    int m_outputDeviceId;
    
    // 音频队列管理
    QQueue<QByteArray> m_audioQueue;
    mutable QMutex m_queueMutex;
    QWaitCondition m_dataAvailable;
};

#endif // PORTAUDIOENGINE_STUB_H
