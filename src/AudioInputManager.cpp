#include "AudioInputManager.hpp"
#include "AudioPermission.h"
#include <QDebug>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QCoreApplication>
#include <cstring>

AudioInputManager::AudioInputManager(QObject *parent)
    : QObject(parent)
    , m_audioSource(nullptr)
    , m_audioDevice(nullptr)
    , m_opusEncoder(std::make_unique<OpusEncoder>())
    , m_webrtcProcessor(std::make_unique<webrtc_apm::WebRTCAudioProcessor>())
    , m_webrtcEnabled(false)
    , m_sampleRate(16000)
    , m_channels(1)
    , m_frameDurationMs(20)
    , m_frameSize(320)
    , m_frameSizeBytes(640)
    , m_initialized(false)
    , m_isRecording(false)
{
}

AudioInputManager::~AudioInputManager()
{
    stopRecording();
    
    if (m_audioSource) {
        delete m_audioSource;
        m_audioSource = nullptr;
    }
}

bool AudioInputManager::initialize(int sampleRate, int channels, int frameDurationMs)
{
    if (m_initialized) {
        qWarning() << "AudioInputManager already initialized";
        return true;
    }
    
    m_sampleRate = sampleRate;
    m_channels = channels;
    m_frameDurationMs = frameDurationMs;
    
    // 计算帧大小
    m_frameSize = OpusEncoder::getFrameSizeForDuration(sampleRate, frameDurationMs);
    m_frameSizeBytes = m_frameSize * sizeof(int16_t) * channels;
    
    qDebug() << "AudioInputManager - 帧大小:" << m_frameSize << "samples," << m_frameSizeBytes << "bytes";
    
    // 设置音频格式
    if (!setupAudioFormat()) {
        qWarning() << "Failed to setup audio format";
        return false;
    }
    
    // 设置Opus编码器
    if (!setupOpusEncoder()) {
        qWarning() << "Failed to setup Opus encoder";
        return false;
    }
    
    // 尝试设置WebRTC（可选，失败不影响录音）
    setupWebRTC();
    
    m_initialized = true;
    qDebug() << "AudioInputManager initialized successfully";
    
    return true;
}

bool AudioInputManager::setupAudioFormat()
{
    // Qt6 音频格式设置
    m_audioFormat.setSampleRate(m_sampleRate);
    m_audioFormat.setChannelCount(m_channels);
    m_audioFormat.setSampleFormat(QAudioFormat::Int16); // 16-bit signed int
    
    qDebug() << "Audio format configured:" << m_audioFormat;
    
    return true;
}

bool AudioInputManager::setupOpusEncoder()
{
    if (!m_opusEncoder->initialize(m_sampleRate, m_channels, OPUS_APPLICATION_VOIP)) {
        qWarning() << "Failed to initialize Opus encoder";
        return false;
    }
    
    // 为语音设置合适的比特率
    // 16kHz: 24-32 kbps
    int bitrate = 24000;
    if (m_sampleRate == 8000) {
        bitrate = 12000;
    } else if (m_sampleRate >= 24000) {
        bitrate = 32000;
    }
    
    m_opusEncoder->setBitrate(bitrate);
    m_opusEncoder->setComplexity(10); // 最高质量
    m_opusEncoder->setVBR(true);
    
    qDebug() << "Opus encoder configured - bitrate:" << bitrate;
    
    return true;
}

bool AudioInputManager::setupWebRTC()
{
    // 只在macOS上使用WebRTC
#ifdef __APPLE__
    if (!m_webrtcProcessor->initialize(m_sampleRate, m_channels)) {
        qWarning() << "Failed to initialize WebRTC processor (continuing without it)";
        m_webrtcEnabled = false;
        return false;
    }
    
    // 默认配置：启用噪声抑制和高通滤波
    webrtc_apm::AudioProcessorConfig config;
    config.echoEnabled = false; // AEC默认关闭，需要参考信号
    config.noiseSuppressionEnabled = true;
    config.noiseLevel = webrtc_apm::NoiseSuppressionLevel::HIGH;
    config.highPassFilterEnabled = true;
    config.gainControl1Enabled = false; // AGC默认关闭
    
    if (!m_webrtcProcessor->applyConfig(config)) {
        qWarning() << "Failed to configure WebRTC processor";
        m_webrtcEnabled = false;
        return false;
    }
    
    m_webrtcProcessor->setStreamDelayMs(40);
    m_webrtcEnabled = true;
    qDebug() << "WebRTC processor initialized and enabled";
    
    return true;
#else
    qDebug() << "WebRTC AEC not available on this platform, using system-level processing";
    m_webrtcEnabled = false;
    return false;
#endif
}

bool AudioInputManager::configureWebRTC(bool enableAEC, bool enableNS, bool enableHighPass)
{
    if (!m_webrtcProcessor->isInitialized()) {
        qWarning() << "WebRTC processor not initialized";
        return false;
    }
    
    webrtc_apm::AudioProcessorConfig config;
    config.echoEnabled = enableAEC;
    config.noiseSuppressionEnabled = enableNS;
    config.noiseLevel = webrtc_apm::NoiseSuppressionLevel::HIGH;
    config.highPassFilterEnabled = enableHighPass;
    config.gainControl1Enabled = false;
    
    if (!m_webrtcProcessor->applyConfig(config)) {
        qWarning() << "Failed to configure WebRTC";
        return false;
    }
    
    qDebug() << "WebRTC configured - AEC:" << enableAEC << "NS:" << enableNS << "HighPass:" << enableHighPass;
    return true;
}

void AudioInputManager::setWebRTCEnabled(bool enabled)
{
    if (enabled && !m_webrtcProcessor->isInitialized()) {
        qWarning() << "Cannot enable WebRTC - processor not initialized";
        return;
    }
    
    m_webrtcEnabled = enabled;
    qDebug() << "WebRTC" << (enabled ? "enabled" : "disabled");
}

bool AudioInputManager::startRecording()
{
    if (!m_initialized) {
        emit errorOccurred("AudioInputManager not initialized");
        return false;
    }
    
    if (m_isRecording) {
        qWarning() << "Already recording";
        return true;
    }
    
    // 检查麦克风权限
    if (!AudioPermission::checkMicrophonePermission()) {
        qWarning() << "Microphone permission not granted";
        emit errorOccurred("麦克风权限未授予，请在系统设置中允许访问");
        
        // 尝试请求权限
        if (!AudioPermission::requestMicrophonePermission()) {
            return false;
        }
    }
    
    // 创建音频输入（Qt6）
    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (inputDevice.isNull()) {
        qWarning() << "AudioInputManager: No audio input device available";
        emit errorOccurred("No audio input device available");
        return false;
    }
    
    qDebug() << "AudioInputManager: Using audio device:" << inputDevice.description();
    qDebug() << "AudioInputManager: Device supports format:" << inputDevice.isFormatSupported(m_audioFormat);
    
    m_audioSource = new QAudioSource(inputDevice, m_audioFormat, this);
    
    qDebug() << "AudioInputManager: Created QAudioSource, buffer size:" << m_audioSource->bufferSize();
    qDebug() << "AudioInputManager: Format:" << m_audioSource->format();
    
    // 开始录音
    m_audioDevice = m_audioSource->start();
    if (!m_audioDevice) {
        qWarning() << "AudioInputManager: Failed to start audio input";
        emit errorOccurred("Failed to start audio input");
        delete m_audioSource;
        m_audioSource = nullptr;
        return false;
    }
    
    qDebug() << "AudioInputManager: Audio device started, state:" << m_audioSource->state();
    qDebug() << "AudioInputManager: Audio device pointer:" << m_audioDevice;
    qDebug() << "AudioInputManager: Device is open:" << m_audioDevice->isOpen();
    qDebug() << "AudioInputManager: Device is readable:" << m_audioDevice->isReadable();
    
    // 连接数据信号
    connect(m_audioDevice, &QIODevice::readyRead, this, &AudioInputManager::onAudioDataReady);
    qDebug() << "AudioInputManager: Connected readyRead signal";
    
    // 连接状态变化信号
    connect(m_audioSource, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
        qDebug() << "AudioInputManager: State changed to:" << state;
        switch(state) {
            case QAudio::ActiveState:
                qDebug() << "  -> Audio is active and processing";
                break;
            case QAudio::SuspendedState:
                qDebug() << "  -> Audio is suspended";
                break;
            case QAudio::StoppedState:
                qDebug() << "  -> Audio is stopped";
                break;
            case QAudio::IdleState:
                qDebug() << "  -> Audio is idle (no data flowing)";
                break;
        }
    });
    
    // 检查音频源的错误
    if (m_audioSource->error() != QAudio::NoError) {
        qWarning() << "AudioInputManager: Audio source error:" << m_audioSource->error();
    }
    
    m_isRecording = true;
    emit recordingStateChanged(true);
    
    qDebug() << "Recording started";
    
    // 强制刷新，尝试触发音频捕获
    QCoreApplication::processEvents();
    
    return true;
}

void AudioInputManager::stopRecording()
{
    if (!m_isRecording) {
        return;
    }
    
    if (m_audioSource) {
        m_audioSource->stop();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioDevice = nullptr;
    }
    
    m_audioBuffer.clear();
    m_isRecording = false;
    emit recordingStateChanged(false);
    
    qDebug() << "Recording stopped";
}

void AudioInputManager::onAudioDataReady()
{
    if (!m_audioDevice) {
        qWarning() << "AudioInputManager: audio device is null";
        return;
    }
    
    // 读取可用的音频数据
    QByteArray data = m_audioDevice->readAll();
    if (data.isEmpty()) {
        qWarning() << "AudioInputManager: no audio data available";
        return;
    }
    
    qDebug() << "AudioInputManager: received" << data.size() << "bytes of audio data";
    processAudioData(data);
}

void AudioInputManager::processAudioData(const QByteArray& rawData)
{
    // 将新数据添加到缓冲区
    m_audioBuffer.append(rawData);
    
    // 处理完整的帧
    while (m_audioBuffer.size() >= m_frameSizeBytes) {
        // 提取一帧数据
        QByteArray frameData = m_audioBuffer.left(m_frameSizeBytes);
        m_audioBuffer.remove(0, m_frameSizeBytes);
        
        // 转换为int16_t数组
        const int16_t* pcmData = reinterpret_cast<const int16_t*>(frameData.constData());
        
        // 如果启用了WebRTC处理
        if (m_webrtcEnabled && m_webrtcProcessor->isInitialized()) {
            // 需要处理为10ms的WebRTC帧
            int webrtcFrameSize = m_webrtcProcessor->getWebRTCFrameSize();
            
            // 如果当前帧大小是WebRTC帧的整数倍
            if (m_frameSize % webrtcFrameSize == 0) {
                std::vector<int16_t> processedData(m_frameSize);
                
                // 分块处理
                int numChunks = m_frameSize / webrtcFrameSize;
                for (int i = 0; i < numChunks; ++i) {
                    const int16_t* chunkInput = pcmData + (i * webrtcFrameSize);
                    int16_t* chunkOutput = processedData.data() + (i * webrtcFrameSize);
                    
                    if (!m_webrtcProcessor->processStream(chunkInput, webrtcFrameSize, chunkOutput)) {
                        qWarning() << "WebRTC processing failed for chunk" << i;
                        // 失败则使用原始数据
                        memcpy(chunkOutput, chunkInput, webrtcFrameSize * sizeof(int16_t));
                    }
                }
                
                // 编码处理后的数据
                encodeAndEmit(processedData.data(), m_frameSize);
            } else {
                // 帧大小不匹配，直接编码原始数据
                qWarning() << "Frame size not compatible with WebRTC, using raw audio";
                encodeAndEmit(pcmData, m_frameSize);
            }
        } else {
            // 不使用WebRTC处理，直接编码
            encodeAndEmit(pcmData, m_frameSize);
        }
    }
}

void AudioInputManager::encodeAndEmit(const int16_t* pcmData, int sampleCount)
{
    qDebug() << "AudioInputManager: encoding" << sampleCount << "samples";
    
    // 使用Opus编码
    QByteArray encodedData = m_opusEncoder->encode(pcmData, sampleCount);
    
    if (!encodedData.isEmpty()) {
        qDebug() << "AudioInputManager: encoded to" << encodedData.size() << "bytes, emitting signal";
        emit audioDataEncoded(encodedData);
    } else {
        qWarning() << "AudioInputManager: Opus encoding returned empty data";
    }
}

bool AudioInputManager::requestMicrophonePermission()
{
    return AudioPermission::requestMicrophonePermission();
}

bool AudioInputManager::checkMicrophonePermission()
{
    return AudioPermission::checkMicrophonePermission();
}

