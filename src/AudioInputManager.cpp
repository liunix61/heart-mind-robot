#include "AudioInputManager.hpp"
#include "AudioPermission.h"
#include <QDebug>
#include <cstring>
#include <portaudio.h>

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
    // 使用 OPUS_APPLICATION_AUDIO 而不是 VOIP，提供更好的音质用于语音识别
    if (!m_opusEncoder->initialize(m_sampleRate, m_channels, OPUS_APPLICATION_AUDIO)) {
        qWarning() << "Failed to initialize Opus encoder";
        return false;
    }
    
    // 设置更高的比特率以提高语音识别准确度
    int bitrate = 32000;  // 提高默认比特率
    if (m_sampleRate == 8000) {
        bitrate = 20000;  // 8kHz时使用20kbps
    } else if (m_sampleRate == 16000) {
        bitrate = 32000;  // 16kHz时使用32kbps（提高以改善识别）
    } else if (m_sampleRate >= 24000) {
        bitrate = 48000;  // 24kHz+时使用48kbps
    }
    
    m_opusEncoder->setBitrate(bitrate);
    m_opusEncoder->setComplexity(10);
    m_opusEncoder->setVBR(true);
    
    // 设置带宽为宽带（WB）或超宽带（SWB）以保留更多语音细节
    if (m_sampleRate >= 16000) {
        m_opusEncoder->setBandwidth(OPUS_BANDWIDTH_WIDEBAND);  // 8kHz带宽
    }
    if (m_sampleRate >= 24000) {
        m_opusEncoder->setBandwidth(OPUS_BANDWIDTH_SUPERWIDEBAND);  // 12kHz带宽
    }
    
    qDebug() << "Opus encoder configured for speech recognition:";
    qDebug() << "  Application: AUDIO (better quality for ASR)";
    qDebug() << "  Bitrate:" << bitrate << "bps";
    qDebug() << "  Sample rate:" << m_sampleRate << "Hz";
    
    return true;
}

bool AudioInputManager::setupWebRTC()
{
    // 禁用WebRTC - 噪声抑制会过度过滤语音，导致识别不完整
    // 直接使用原始音频可以获得更好的语音识别效果
    qDebug() << "WebRTC disabled - using raw audio for better speech recognition";
    m_webrtcEnabled = false;
    return false;
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
            QString permissionMsg = QString("麦克风权限未授予\n\n"
                                           "请按以下步骤操作：\n\n"
                                           "1. 前往 系统偏好设置 -> 安全性与隐私 -> 隐私 -> 麦克风\n"
                                           "2. 确保 HeartMindRobot 已勾选\n\n"
                                           "如果列表中没有该应用：\n"
                                           "3. 打开终端，执行以下命令：\n"
                                           "   sudo xattr -rd com.apple.quarantine /Applications/HeartMindRobot.app\n"
                                           "4. 重新启动应用\n\n"
                                           "注意：未签名的应用可能需要额外的安全设置才能访问麦克风。");
            emit errorOccurred(permissionMsg);
            return false;
        }
    }
    
    qDebug() << "Microphone permission OK, proceeding to open audio stream...";
    
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
        QString errorMsg = Pa_GetErrorText(err);
        qCritical() << "Failed to open audio stream:" << errorMsg;
        qCritical() << "PortAudio Error Code:" << err;
        qCritical() << "Device:" << deviceInfo->name;
        qCritical() << "Sample Rate:" << m_sampleRate;
        qCritical() << "Channels:" << m_channels;
        
        // 提供更详细的错误信息
        QString userMsg = QString("音频流打开失败\n\n"
                                 "错误信息: %1\n"
                                 "错误代码: %2\n"
                                 "设备: %3\n\n"
                                 "可能的原因：\n"
                                 "1. 麦克风权限未正确授予\n"
                                 "2. 应用未签名，被系统安全限制\n"
                                 "3. 其他应用正在占用麦克风\n\n"
                                 "解决方法：\n"
                                 "1. 前往 系统偏好设置 -> 安全性与隐私 -> 隐私 -> 麦克风\n"
                                 "2. 确保 HeartMindRobot 已勾选\n"
                                 "3. 如果列表中没有该应用，请先执行：\n"
                                 "   sudo xattr -rd com.apple.quarantine /Applications/HeartMindRobot.app")
                         .arg(errorMsg)
                         .arg(err)
                         .arg(deviceInfo->name);
        
        emit errorOccurred(userMsg);
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
