#include "AudioInputManager.hpp"
#include "AudioPermission.h"
#include <QDebug>
#include <cstring>

AudioInputManager::AudioInputManager(QObject *parent)
    : QObject(parent)
    , m_stream(nullptr)
    , m_paInitialized(false)
    , m_opusEncoder(std::make_unique<OpusEncoder>())
    , m_webrtcProcessor(std::make_unique<webrtc_apm::WebRTCAudioProcessor>())
    , m_webrtcEnabled(false)
    , m_sampleRate(16000)
    , m_channels(1)
    , m_frameDurationMs(20)
    , m_frameSize(320)
    , m_initialized(false)
    , m_isRecording(false)
{
}

AudioInputManager::~AudioInputManager()
{
    stopRecording();
    
    if (m_paInitialized) {
        Pa_Terminate();
        m_paInitialized = false;
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
    
    qDebug() << "AudioInputManager - 帧大小:" << m_frameSize << "samples";
    qDebug() << "AudioInputManager - 采样率:" << m_sampleRate << "Hz";
    qDebug() << "AudioInputManager - 声道:" << m_channels;
    
    // 初始化PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        qWarning() << "Failed to initialize PortAudio:" << Pa_GetErrorText(err);
        return false;
    }
    m_paInitialized = true;
    qDebug() << "PortAudio initialized successfully";
    
    // 列出可用的音频设备
    int numDevices = Pa_GetDeviceCount();
    qDebug() << "Available audio devices:" << numDevices;
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxInputChannels > 0) {
            qDebug() << "  [" << i << "]" << deviceInfo->name 
                     << "- Input channels:" << deviceInfo->maxInputChannels;
        }
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

bool AudioInputManager::setupOpusEncoder()
{
    if (!m_opusEncoder->initialize(m_sampleRate, m_channels, OPUS_APPLICATION_VOIP)) {
        qWarning() << "Failed to initialize Opus encoder";
        return false;
    }
    
    // 为语音设置合适的比特率
    int bitrate = 24000;
    if (m_sampleRate == 8000) {
        bitrate = 12000;
    } else if (m_sampleRate >= 24000) {
        bitrate = 32000;
    }
    
    m_opusEncoder->setBitrate(bitrate);
    m_opusEncoder->setComplexity(10);
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
    config.echoEnabled = false;
    config.noiseSuppressionEnabled = true;
    config.noiseLevel = webrtc_apm::NoiseSuppressionLevel::HIGH;
    config.highPassFilterEnabled = true;
    config.gainControl1Enabled = false;
    
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
    qDebug() << "WebRTC not available on this platform";
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
    
    // 检查并请求麦克风权限
    qDebug() << "AudioInputManager: Checking microphone permission...";
    bool hasPermission = AudioPermission::checkMicrophonePermission();
    qDebug() << "AudioInputManager: Permission status:" << hasPermission;
    
    if (!hasPermission) {
        qWarning() << "Microphone permission not granted, requesting...";
        hasPermission = AudioPermission::requestMicrophonePermission();
        qDebug() << "AudioInputManager: Permission after request:" << hasPermission;
        
        if (!hasPermission) {
            emit errorOccurred("麦克风权限未授予，请在系统设置中允许访问");
            return false;
        }
    }
    
    // 配置输入参数
    PaStreamParameters inputParameters;
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
        qWarning() << "No default input device found";
        emit errorOccurred("No audio input device available");
        return false;
    }
    
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(inputParameters.device);
    qDebug() << "Using input device:" << deviceInfo->name;
    qDebug() << "Device sample rate:" << deviceInfo->defaultSampleRate;
    qDebug() << "Device input channels:" << deviceInfo->maxInputChannels;
    
    inputParameters.channelCount = m_channels;
    inputParameters.sampleFormat = paInt16;
    inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    
    // 打开音频流
    PaError err = Pa_OpenStream(
        &m_stream,
        &inputParameters,
        NULL,  // 无输出
        m_sampleRate,
        m_frameSize,  // 每次回调的帧数
        paClipOff,
        &AudioInputManager::audioCallback,
        this  // 用户数据指针，传递this
    );
    
    if (err != paNoError) {
        qWarning() << "Failed to open audio stream:" << Pa_GetErrorText(err);
        emit errorOccurred(QString("Failed to open audio stream: %1").arg(Pa_GetErrorText(err)));
        return false;
    }
    
    // 启动音频流
    err = Pa_StartStream(m_stream);
    if (err != paNoError) {
        qWarning() << "Failed to start audio stream:" << Pa_GetErrorText(err);
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
        emit errorOccurred(QString("Failed to start audio stream: %1").arg(Pa_GetErrorText(err)));
        return false;
    }
    
    m_isRecording = true;
    emit recordingStateChanged(true);
    
    qDebug() << "Recording started successfully with PortAudio";
    qDebug() << "Stream is active:" << Pa_IsStreamActive(m_stream);
    
    return true;
}

void AudioInputManager::stopRecording()
{
    if (!m_isRecording) {
        return;
    }
    
    if (m_stream) {
        PaError err = Pa_StopStream(m_stream);
        if (err != paNoError) {
            qWarning() << "Error stopping stream:" << Pa_GetErrorText(err);
        }
        
        err = Pa_CloseStream(m_stream);
        if (err != paNoError) {
            qWarning() << "Error closing stream:" << Pa_GetErrorText(err);
        }
        
        m_stream = nullptr;
    }
    
    m_isRecording = false;
    emit recordingStateChanged(false);
    
    qDebug() << "Recording stopped";
}

// PortAudio回调函数 - 在音频线程中调用
int AudioInputManager::audioCallback(
    const void *inputBuffer,
    void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData)
{
    AudioInputManager* self = static_cast<AudioInputManager*>(userData);
    const int16_t* input = static_cast<const int16_t*>(inputBuffer);
    
    // 检查输入
    if (!input || !self) {
        return paContinue;
    }
    
    // 检查状态标志
    if (statusFlags & paInputOverflow) {
        qWarning() << "PortAudio: Input overflow detected";
    }
    
    // 处理音频数据
    self->processAudioData(input, framesPerBuffer);
    
    return paContinue;
}

void AudioInputManager::processAudioData(const int16_t* pcmData, int sampleCount)
{
    if (!pcmData || sampleCount != m_frameSize) {
        return;
    }
    
    // 如果启用了WebRTC处理
    if (m_webrtcEnabled && m_webrtcProcessor->isInitialized()) {
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
                    // 失败则使用原始数据
                    memcpy(chunkOutput, chunkInput, webrtcFrameSize * sizeof(int16_t));
                }
            }
            
            // 编码处理后的数据
            encodeAndEmit(processedData.data(), m_frameSize);
        } else {
            // 帧大小不匹配，直接编码原始数据
            encodeAndEmit(pcmData, m_frameSize);
        }
    } else {
        // 不使用WebRTC处理，直接编码
        encodeAndEmit(pcmData, m_frameSize);
    }
}

void AudioInputManager::encodeAndEmit(const int16_t* pcmData, int sampleCount)
{
    // 使用Opus编码
    QByteArray encodedData = m_opusEncoder->encode(pcmData, sampleCount);
    
    if (!encodedData.isEmpty()) {
        // 发射信号（Qt会自动处理跨线程的信号）
        emit audioDataEncoded(encodedData);
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
