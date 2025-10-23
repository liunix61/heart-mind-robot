#ifndef OPUSDECODER_H
#define OPUSDECODER_H

#include <QByteArray>
#include <QObject>
#include <opus/opus.h>

class OpusDecoder : public QObject
{
    Q_OBJECT

public:
    explicit OpusDecoder(QObject *parent = nullptr);
    ~OpusDecoder();

    // 初始化解码器
    bool initialize(int sampleRate = 24000, int channels = 1);
    
    // 解码Opus数据为PCM
    QByteArray decode(const QByteArray &opusData);
    
    // 检查是否已初始化
    bool isInitialized() const { return m_decoder != nullptr; }
    
    // 获取采样率
    int getSampleRate() const { return m_sampleRate; }
    
    // 获取声道数
    int getChannels() const { return m_channels; }

private:
    ::OpusDecoder *m_decoder;  // opus库的解码器类型，使用::前缀避免与类名冲突
    int m_sampleRate;
    int m_channels;
    bool m_initialized;
    
    // 内部缓冲区
    QByteArray m_pcmBuffer;
};

#endif // OPUSDECODER_H
