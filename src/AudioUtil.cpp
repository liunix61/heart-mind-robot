#include "AudioUtil.h"
#include "Log_util.h"
#include "platform_config.h"

AudioPlayer::AudioPlayer() 
    : m_opusDecoder(nullptr)
{
    WINDOWS_SPECIFIC(
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        audioPlayer = nullptr;
    )
    
    // 初始化Opus解码器
    m_opusDecoder = new OpusDecoder();
    if (!m_opusDecoder->initialize(24000, 1)) {
        CF_LOG_ERROR("Failed to initialize Opus decoder");
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
    }
}

AudioPlayer::~AudioPlayer() {
    WINDOWS_SPECIFIC(
        CoUninitialize();
    )
    
    // 清理Opus解码器
    if (m_opusDecoder) {
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
    }
}

void AudioPlayer::playAudio(const char *filePath) {
    WINDOWS_SPECIFIC(
        // Windows 实现
        PlaySound(filePath, NULL, SND_FILENAME | SND_ASYNC);
    )
}

void AudioPlayer::playAudio(const char *audioData, size_t dataSize) {
    WINDOWS_SPECIFIC(
        // Windows 实现
        PlaySound((LPCSTR)audioData, NULL, SND_MEMORY | SND_ASYNC);
    )
}

void AudioPlayer::playReceivedAudioData(const QByteArray &audioData) {
    if (audioData.isEmpty()) {
        CF_LOG_INFO("Empty audio data received, skipping playback");
        return;
    }
    
    CF_LOG_DEBUG("Playing received audio data, size: %zu bytes", audioData.size());
    
    // 检查Opus解码器是否可用
    if (!m_opusDecoder || !m_opusDecoder->isInitialized()) {
        CF_LOG_ERROR("Opus decoder not available, cannot play audio");
        return;
    }
    
    // 解码Opus数据为PCM
    QByteArray pcmData = m_opusDecoder->decode(audioData);
    if (pcmData.isEmpty()) {
        CF_LOG_ERROR("Failed to decode Opus audio data");
        return;
    }
    
    CF_LOG_DEBUG("Opus decoded successfully, PCM size: %zu bytes", pcmData.size());
    
    // 播放解码后的PCM数据
    playAudio(pcmData.constData(), pcmData.size());
}