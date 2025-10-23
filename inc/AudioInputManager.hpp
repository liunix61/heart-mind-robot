#ifndef AUDIOINPUTMANAGER_HPP
#define AUDIOINPUTMANAGER_HPP

#include <QObject>
#include <QAudioSource>
#include <QAudioFormat>
#include <QIODevice>
#include <QMediaDevices>
#include <QAudioDevice>
#include <memory>
#include "OpusEncoder.hpp"
#include "WebRTCAudioProcessor.hpp"

/**
 * 音频输入管理器
 * 负责：
 * 1. 麦克风音频采集
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
    
    // 获取音频格式
    QAudioFormat getAudioFormat() const { return m_audioFormat; }
    
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

private slots:
    void onAudioDataReady();

private:
    // 音频输入相关
    QAudioSource* m_audioSource;
    QIODevice* m_audioDevice;
    QAudioFormat m_audioFormat;
    
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
    int m_frameSizeBytes;     // 字节数
    
    // 音频缓冲
    QByteArray m_audioBuffer;
    
    // 状态
    bool m_initialized;
    bool m_isRecording;
    
    // 内部方法
    bool setupAudioFormat();
    bool setupOpusEncoder();
    bool setupWebRTC();
    void processAudioData(const QByteArray& rawData);
    void encodeAndEmit(const int16_t* pcmData, int sampleCount);
};

#endif // AUDIOINPUTMANAGER_HPP

