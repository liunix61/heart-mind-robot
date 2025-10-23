#ifndef OPUSENCODER_HPP
#define OPUSENCODER_HPP

#include <QByteArray>
#include <QObject>
#include <opus/opus.h>

class OpusEncoder : public QObject
{
    Q_OBJECT

public:
    explicit OpusEncoder(QObject *parent = nullptr);
    ~OpusEncoder();

    // 初始化编码器
    bool initialize(int sampleRate = 16000, int channels = 1, int application = OPUS_APPLICATION_VOIP);
    
    // 编码PCM数据为Opus
    QByteArray encode(const int16_t* pcmData, int frameSize);
    
    // 设置比特率
    bool setBitrate(int bitrate);
    
    // 设置复杂度 (0-10，默认10最高质量)
    bool setComplexity(int complexity);
    
    // 设置VBR (Variable Bitrate)
    bool setVBR(bool enabled);
    
    // 检查是否已初始化
    bool isInitialized() const { return m_encoder != nullptr; }
    
    // 获取采样率
    int getSampleRate() const { return m_sampleRate; }
    
    // 获取声道数
    int getChannels() const { return m_channels; }
    
    // 获取帧大小（样本数）
    // Opus支持的帧长度：2.5, 5, 10, 20, 40, 60 ms
    // 对于16kHz采样率：40, 80, 160, 320, 640, 960 samples
    static int getFrameSizeForDuration(int sampleRate, float durationMs);

private:
    ::OpusEncoder *m_encoder;  // opus库的编码器类型，使用::前缀避免与类名冲突
    int m_sampleRate;
    int m_channels;
    int m_application;
    bool m_initialized;
    
    // 编码缓冲区
    QByteArray m_opusBuffer;
};

#endif // OPUSENCODER_HPP

