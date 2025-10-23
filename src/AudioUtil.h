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

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    
    void playAudio(const char *filePath);
    void playAudio(const char *audioData, size_t dataSize);
    
    // 新增：播放接收到的Opus编码音频数据 - 异步解码并播放
    void playReceivedAudioData(const QByteArray &audioData);

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
};

#endif // AUDIOUTIL_H
