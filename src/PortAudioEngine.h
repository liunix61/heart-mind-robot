#ifndef PORTAUDIOENGINE_H
#define PORTAUDIOENGINE_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <QThread>

#include <portaudio.h>

/**
 * @brief 基于PortAudio的高性能流式音频播放引擎
 * 
 * 特性：
 * - 跨平台支持（Windows/macOS/Linux）
 * - 低延迟流式播放
 * - 自动设备选择
 * - 智能重采样
 * - 异步队列管理
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

private slots:
    void processAudioQueue();

private:
    // PortAudio回调函数
    static int audioCallback(const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo *timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData);
    
    // 内部处理函数
    void handleAudioCallback(void *outputBuffer, unsigned long framesPerBuffer);
    bool setupAudioStream();
    void cleanupAudioStream();
    
    // 重采样相关
    bool initializeResampler(int inputRate, int outputRate);
    QByteArray resampleAudio(const QByteArray &inputData);
    
    // 成员变量
    bool m_initialized;
    bool m_isPlaying;
    PaStream *m_stream;
    
    // 音频参数
    int m_sampleRate;
    int m_channels;
    int m_outputDeviceId;
    
    // 音频队列管理
    QQueue<QByteArray> m_audioQueue;
    mutable QMutex m_queueMutex;
    QWaitCondition m_dataAvailable;
    
    // 重采样相关
    bool m_needsResampling;
    int m_deviceSampleRate;
    double m_resampleRatio;
    QByteArray m_resampleBuffer;
    
    // 音频累积缓冲区（线程安全）
    QByteArray m_accumulatedData;
    mutable QMutex m_accumulatedDataMutex;
    
    // 播放控制
    QThread *m_processingThread;
    bool m_shouldStop;
};

#endif // PORTAUDIOENGINE_H
