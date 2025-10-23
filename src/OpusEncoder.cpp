#include "OpusEncoder.hpp"
#include <QDebug>

OpusEncoder::OpusEncoder(QObject *parent)
    : QObject(parent)
    , m_encoder(nullptr)
    , m_sampleRate(16000)
    , m_channels(1)
    , m_application(OPUS_APPLICATION_VOIP)
    , m_initialized(false)
{
    // 预分配编码缓冲区（最大可能大小）
    m_opusBuffer.resize(4000); // Opus规范推荐的最大包大小
}

OpusEncoder::~OpusEncoder()
{
    if (m_encoder) {
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
    }
}

bool OpusEncoder::initialize(int sampleRate, int channels, int application)
{
    if (m_encoder) {
        qWarning() << "OpusEncoder already initialized";
        return true;
    }
    
    // 验证参数
    if (sampleRate != 8000 && sampleRate != 12000 && 
        sampleRate != 16000 && sampleRate != 24000 && 
        sampleRate != 48000) {
        qWarning() << "Unsupported sample rate:" << sampleRate;
        qWarning() << "Supported rates: 8000, 12000, 16000, 24000, 48000";
        return false;
    }
    
    if (channels != 1 && channels != 2) {
        qWarning() << "Unsupported channel count:" << channels;
        return false;
    }
    
    m_sampleRate = sampleRate;
    m_channels = channels;
    m_application = application;
    
    int error = 0;
    m_encoder = opus_encoder_create(m_sampleRate, m_channels, m_application, &error);
    
    if (error != OPUS_OK || !m_encoder) {
        qWarning() << "Failed to create Opus encoder:" << opus_strerror(error);
        m_encoder = nullptr;
        return false;
    }
    
    // 设置默认参数
    // 设置比特率为自动
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(OPUS_AUTO));
    
    // 设置VBR（可变比特率）
    opus_encoder_ctl(m_encoder, OPUS_SET_VBR(1));
    
    // 设置复杂度（0-10，10为最高质量）
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(10));
    
    // 启用FEC（前向纠错）以提高抗丢包能力
    opus_encoder_ctl(m_encoder, OPUS_SET_INBAND_FEC(1));
    
    // 设置DTX（不连续传输）以节省带宽
    opus_encoder_ctl(m_encoder, OPUS_SET_DTX(0)); // 语音场景建议关闭
    
    m_initialized = true;
    
    qDebug() << "OpusEncoder initialized successfully";
    qDebug() << "  Sample rate:" << m_sampleRate;
    qDebug() << "  Channels:" << m_channels;
    qDebug() << "  Application:" << (application == OPUS_APPLICATION_VOIP ? "VOIP" : 
                                      application == OPUS_APPLICATION_AUDIO ? "AUDIO" : "RESTRICTED_LOWDELAY");
    
    return true;
}

QByteArray OpusEncoder::encode(const int16_t* pcmData, int frameSize)
{
    if (!m_encoder || !m_initialized) {
        qWarning() << "OpusEncoder not initialized";
        return QByteArray();
    }
    
    if (!pcmData) {
        qWarning() << "Invalid PCM data";
        return QByteArray();
    }
    
    // 编码
    int encodedBytes = opus_encode(
        m_encoder,
        pcmData,
        frameSize,
        reinterpret_cast<unsigned char*>(m_opusBuffer.data()),
        m_opusBuffer.size()
    );
    
    if (encodedBytes < 0) {
        qWarning() << "Opus encoding failed:" << opus_strerror(encodedBytes);
        return QByteArray();
    }
    
    if (encodedBytes == 0) {
        // DTX激活，没有数据需要发送
        return QByteArray();
    }
    
    // 返回编码后的数据
    return QByteArray(m_opusBuffer.constData(), encodedBytes);
}

bool OpusEncoder::setBitrate(int bitrate)
{
    if (!m_encoder || !m_initialized) {
        qWarning() << "OpusEncoder not initialized";
        return false;
    }
    
    int error = opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(bitrate));
    if (error != OPUS_OK) {
        qWarning() << "Failed to set bitrate:" << opus_strerror(error);
        return false;
    }
    
    qDebug() << "Bitrate set to:" << bitrate;
    return true;
}

bool OpusEncoder::setComplexity(int complexity)
{
    if (!m_encoder || !m_initialized) {
        qWarning() << "OpusEncoder not initialized";
        return false;
    }
    
    if (complexity < 0 || complexity > 10) {
        qWarning() << "Complexity must be between 0 and 10";
        return false;
    }
    
    int error = opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(complexity));
    if (error != OPUS_OK) {
        qWarning() << "Failed to set complexity:" << opus_strerror(error);
        return false;
    }
    
    qDebug() << "Complexity set to:" << complexity;
    return true;
}

bool OpusEncoder::setVBR(bool enabled)
{
    if (!m_encoder || !m_initialized) {
        qWarning() << "OpusEncoder not initialized";
        return false;
    }
    
    int error = opus_encoder_ctl(m_encoder, OPUS_SET_VBR(enabled ? 1 : 0));
    if (error != OPUS_OK) {
        qWarning() << "Failed to set VBR:" << opus_strerror(error);
        return false;
    }
    
    qDebug() << "VBR" << (enabled ? "enabled" : "disabled");
    return true;
}

int OpusEncoder::getFrameSizeForDuration(int sampleRate, float durationMs)
{
    // Opus支持的帧长度：2.5, 5, 10, 20, 40, 60 ms
    // 计算对应的样本数
    return static_cast<int>((sampleRate * durationMs) / 1000.0f);
}

