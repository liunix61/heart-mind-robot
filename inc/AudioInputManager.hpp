#ifndef AUDIOINPUTMANAGER_HPP
#define AUDIOINPUTMANAGER_HPP

#include <QObject>
#include <QByteArray>
#include <memory>
#include "PlatformConfig.hpp"

// 所有平台都使用PortAudio
#include <portaudio.h>

#include "OpusEncoder.hpp"
#include "WebRTCAudioProcessor.hpp"

/**
 * 音频输入管理器（基于PortAudio）
 * 负责：
 * 1. 麦克风音频采集（使用PortAudio callback模式）
 * 2. WebRTC音频处理（AEC、NS等）
 * 3. Opus编码
 * 4. 发送编码后的音频数据
 */
class AudioInputManager : public QObject
{
    Q_OBJECT

public:
    explicit AudioInputManager(QObject *parent = nullptr);
    ~AudioInputManager();
    
    // 初始化（采样率、声道、帧时长ms）
    bool initialize(int sampleRate = 16000, int channels = 1, int frameDurationMs = 20);
    
    // 开始录音
    bool startRecording();
    
    // 停止录音
    void stopRecording();
    
    // 检查是否正在录音
    bool isRecording() const { return m_isRecording; }
    
    // 启用/禁用WebRTC处理
    void setWebRTCEnabled(bool enabled);
    bool isWebRTCEnabled() const { return m_webrtcEnabled; }
    
    // 配置WebRTC
    bool configureWebRTC(bool enableAEC, bool enableNS, bool enableHighPass);
    
    // 请求麦克风权限（macOS特定）
    static bool requestMicrophonePermission();
    
    // 检查麦克风权限状态
    static bool checkMicrophonePermission();

signals:
    // 编码后的音频数据准备好
    void audioDataEncoded(const QByteArray& encodedData);
    
    // 录音状态改变
    void recordingStateChanged(bool isRecording);
    
    // 错误信号
    void errorOccurred(const QString& error);

private:
    // 音频回调函数（必须是静态的）
    // 所有平台都使用PortAudio
    static int audioCallback(
        const void *inputBuffer,
        void *outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData);
    
    // 处理音频数据
    void processAudioData(const int16_t* pcmData, int sampleCount);
    
    // 编码并发送
    void encodeAndEmit(const int16_t* pcmData, int sampleCount);
    
    // 音频相关
    // 所有平台都使用PortAudio
    PaStream* m_stream;
    bool m_paInitialized;
    
    // Opus编码器
    std::unique_ptr<OpusEncoder> m_opusEncoder;
    
    // WebRTC音频处理器
    std::unique_ptr<webrtc_apm::WebRTCAudioProcessor> m_webrtcProcessor;
    bool m_webrtcEnabled;
    
    // 音频参数
    int m_sampleRate;
    int m_channels;
    int m_frameDurationMs;
    int m_frameSize;          // 样本数
    
    // 状态
    bool m_initialized;
    bool m_isRecording;
    
    // 内部方法
    bool setupOpusEncoder();
    bool setupWebRTC();
};

#endif // AUDIOINPUTMANAGER_HPP
