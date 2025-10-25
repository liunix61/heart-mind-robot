#ifndef AUDIOUTIL_H
#define AUDIOUTIL_H

#include "Log_util.h"
#include "platform_config.h"
#include <QByteArray>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include "OpusDecoder.h"

// 音频播放工作线程
class AudioPlaybackThread : public QThread {
    Q_OBJECT
public:
    AudioPlaybackThread(QObject *parent = nullptr);
    ~AudioPlaybackThread();
    
    void enqueueAudio(const QByteArray &audioData);
    void stopPlayback();
    void clearAudioQueue();  // 新增：清空音频队列
    
signals:
    // 当音频解码完成后发射，用于口型同步
    void audioDecoded(const QByteArray &pcmData);
    
protected:
    void run() override;
    
private:
    QQueue<QByteArray> m_audioQueue;
    QMutex m_queueMutex;
    volatile bool m_running;
    OpusDecoder *m_opusDecoder;
    void *m_audioEngineManager;  // AudioEngineManager* (macOS特定)
    
    void processAudioData(const QByteArray &audioData);
};

class AudioPlayer : public QObject {
    Q_OBJECT
    
public:
    AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();
    
    void playAudio(const char *filePath);
    void playAudio(const char *audioData, size_t dataSize);
    
    // 新增：播放接收到的Opus编码音频数据 - 异步解码并播放
    void playReceivedAudioData(const QByteArray &audioData);
    
    // 新增：清空音频队列（用于中断对话）
    void clearAudioQueue();
    
    // 获取音频播放线程（用于连接信号）
    AudioPlaybackThread* getPlaybackThread() { return m_playbackThread; }

signals:
    // 转发解码后的音频数据
    void audioDecoded(const QByteArray &pcmData);

private slots:
    // 处理解码后的PCM数据播放
    void onPCMDataReady(const QByteArray &pcmData);

private:
    // 音频播放工作线程（包含Opus解码器）
    AudioPlaybackThread *m_playbackThread;
    
    // 平台特定的成员变量
    MACOS_SPECIFIC(
        void* audioPlayer; // AVAudioPlayer for macOS (使用void*避免C++编译问题)
    )
    
    WINDOWS_SPECIFIC(
        // Windows 音频播放器成员
        void* audioPlayer;
    )
    
    // Opus解码器
    OpusDecoder *m_opusDecoder;
};

#endif // AUDIOUTIL_H
