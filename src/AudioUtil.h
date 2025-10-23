#ifndef AUDIOUTIL_H
#define AUDIOUTIL_H

#include "Log_util.h"
#include "platform_config.h"
#include <QByteArray>
#include "OpusDecoder.h"

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    
    void playAudio(const char *filePath);
    void playAudio(const char *audioData, size_t dataSize);
    
    // 新增：播放接收到的音频数据（支持Opus格式）
    void playReceivedAudioData(const QByteArray &audioData);

private:
    // Opus解码器
    OpusDecoder *m_opusDecoder;
    
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
