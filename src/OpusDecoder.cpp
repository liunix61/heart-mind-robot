#include "OpusDecoder.h"
#include "LogUtil.h"
#include <QDebug>

OpusDecoder::OpusDecoder(QObject *parent)
    : QObject(parent)
    , m_decoder(nullptr)
    , m_sampleRate(24000)
    , m_channels(1)
    , m_initialized(false)
{
}

OpusDecoder::~OpusDecoder()
{
    if (m_decoder) {
        opus_decoder_destroy(m_decoder);
        m_decoder = nullptr;
    }
}

bool OpusDecoder::initialize(int sampleRate, int channels)
{
    if (m_initialized) {
        CF_LOG_INFO("OpusDecoder already initialized");
        return true;
    }
    
    m_sampleRate = sampleRate;
    m_channels = channels;
    
    int error;
    m_decoder = opus_decoder_create(sampleRate, channels, &error);
    
    if (error != OPUS_OK) {
        CF_LOG_ERROR("Failed to create Opus decoder: %s", opus_strerror(error));
        m_decoder = nullptr;
        return false;
    }
    
    m_initialized = true;
    CF_LOG_INFO("OpusDecoder initialized successfully (sample rate: %d, channels: %d)", sampleRate, channels);
    return true;
}

QByteArray OpusDecoder::decode(const QByteArray &opusData)
{
    if (!m_initialized || !m_decoder) {
        CF_LOG_ERROR("OpusDecoder not initialized");
        return QByteArray();
    }
    
    if (opusData.isEmpty()) {
        CF_LOG_INFO("Empty Opus data received");
        return QByteArray();
    }
    
    // 计算最大可能的PCM输出大小
    // Opus帧大小通常是20ms，对于24kHz采样率，每帧有480个样本
    int maxFrameSize = m_sampleRate * 0.02; // 20ms
    int maxSamples = maxFrameSize * m_channels;
    
    // 准备PCM缓冲区（使用float类型，因为opus_decode期望float*）
    QByteArray pcmData(maxSamples * sizeof(opus_int16), 0);
    opus_int16 *pcmBuffer = reinterpret_cast<opus_int16*>(pcmData.data());
    
    CF_LOG_INFO(">>> OpusDecoder: decoding %d bytes of Opus data", opusData.size());
    CF_LOG_INFO(">>> Max frame size: %d, max samples: %d", maxFrameSize, maxSamples);
    
    // 解码Opus数据
    int decodedSamples = opus_decode(
        m_decoder,
        reinterpret_cast<const unsigned char*>(opusData.constData()),
        opusData.size(),
        pcmBuffer,
        maxSamples,
        0 // 不使用FEC
    );
    
    if (decodedSamples < 0) {
        CF_LOG_ERROR("Opus decode failed: %s", opus_strerror(decodedSamples));
        return QByteArray();
    }
    
    CF_LOG_INFO(">>> Opus decoded %d samples successfully", decodedSamples);
    
    // 检查前10个样本
    CF_LOG_INFO(">>> First 10 PCM samples after decode:");
    for (int i = 0; i < std::min(10, decodedSamples); i++) {
        CF_LOG_INFO(">>>   pcm[%d] = %d", i, pcmBuffer[i]);
    }
    
    // 调整PCM数据大小
    pcmData.resize(decodedSamples * m_channels * sizeof(opus_int16));
    
    CF_LOG_INFO(">>> Final PCM data size: %d bytes", pcmData.size());
    return pcmData;
}
