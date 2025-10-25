#include "PortAudioEngine.h"
#include "Log_util.h"
#include <QDebug>
#include <QMutexLocker>
#include <QThread>

// 使用PortAudio
#define PORTAUDIO_ENABLED 1

PortAudioEngine::PortAudioEngine(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
    , m_isPlaying(false)
    , m_stream(nullptr)
    , m_sampleRate(24000)
    , m_channels(1)
    , m_outputDeviceId(-1)
    , m_needsResampling(false)
    , m_deviceSampleRate(24000)
    , m_resampler(nullptr)
    , m_processingThread(nullptr)
    , m_shouldStop(false)
{
    CF_LOG_INFO("PortAudioEngine: Constructor called");
}

PortAudioEngine::~PortAudioEngine()
{
    stopPlayback();
    cleanupAudioStream();
    
    if (m_resampler) {
        // 清理重采样器
        // src_delete(static_cast<SRC_STATE*>(m_resampler));
        m_resampler = nullptr;
    }
    
    CF_LOG_INFO("PortAudioEngine: Destructor completed");
}

bool PortAudioEngine::initialize(int sampleRate, int channels)
{
    CF_LOG_INFO("PortAudioEngine: Initializing with sample rate: %d, channels: %d", sampleRate, channels);
    
    if (m_initialized) {
        CF_LOG_INFO("PortAudioEngine: Already initialized");
        return true;
    }
    
#if PORTAUDIO_ENABLED
    CF_LOG_INFO("PortAudioEngine: Attempting to initialize PortAudio library...");
    
    // 初始化PortAudio
    PaError error = Pa_Initialize();
    if (error != paNoError) {
        CF_LOG_ERROR("PortAudioEngine: Failed to initialize PortAudio: %s", Pa_GetErrorText(error));
        CF_LOG_ERROR("PortAudioEngine: PortAudio initialization failed, error code: %d", error);
        return false;
    }
    
    CF_LOG_INFO("PortAudioEngine: PortAudio library initialized successfully");
    
    m_sampleRate = sampleRate;
    m_channels = channels;
    
    // 获取默认输出设备
    m_outputDeviceId = Pa_GetDefaultOutputDevice();
    if (m_outputDeviceId == paNoDevice) {
        CF_LOG_ERROR("PortAudioEngine: No default output device found");
        Pa_Terminate();
        return false;
    }
    
    // 获取设备信息
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(m_outputDeviceId);
    if (deviceInfo) {
        m_deviceSampleRate = static_cast<int>(deviceInfo->defaultSampleRate);
        CF_LOG_INFO("PortAudioEngine: Output device: %s, sample rate: %d", 
                    deviceInfo->name, m_deviceSampleRate);
        
        // 检查是否需要重采样
        m_needsResampling = (m_deviceSampleRate != m_sampleRate);
        if (m_needsResampling) {
            CF_LOG_INFO("PortAudioEngine: Resampling required: %d -> %d Hz", 
                       m_sampleRate, m_deviceSampleRate);
            if (!initializeResampler(m_sampleRate, m_deviceSampleRate)) {
                CF_LOG_ERROR("PortAudioEngine: Failed to initialize resampler");
                Pa_Terminate();
                return false;
            }
        }
    }
    
    // 设置音频流
    if (!setupAudioStream()) {
        CF_LOG_ERROR("PortAudioEngine: Failed to setup audio stream");
        Pa_Terminate();
        return false;
    }
    
    m_initialized = true;
    CF_LOG_INFO("PortAudioEngine: Initialization completed successfully");
    return true;
#else
    // 存根实现：直接初始化
    m_sampleRate = sampleRate;
    m_channels = channels;
    m_outputDeviceId = 0;
    m_deviceSampleRate = sampleRate;
    m_needsResampling = false;
    m_initialized = true;
    
    CF_LOG_INFO("PortAudioEngine: Stub implementation initialized successfully");
    return true;
#endif
}

bool PortAudioEngine::setupAudioStream()
{
    PaStreamParameters outputParams;
    outputParams.device = m_outputDeviceId;
    outputParams.channelCount = m_channels;
    outputParams.sampleFormat = paInt16;
    outputParams.suggestedLatency = Pa_GetDeviceInfo(m_outputDeviceId)->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;
    
    // 使用设备采样率或目标采样率
    double streamSampleRate = m_needsResampling ? m_deviceSampleRate : m_sampleRate;
    
    PaError error = Pa_OpenStream(
        &m_stream,
        nullptr, // 无输入
        &outputParams,
        streamSampleRate,
        256, // 小缓冲区，低延迟
        paClipOff, // 不裁剪
        audioCallback,
        this // 用户数据
    );
    
    if (error != paNoError) {
        CF_LOG_ERROR("PortAudioEngine: Failed to open stream: %s", Pa_GetErrorText(error));
        return false;
    }
    
    CF_LOG_INFO("PortAudioEngine: Audio stream created successfully");
    return true;
}

bool PortAudioEngine::startPlayback()
{
    if (!m_initialized || m_isPlaying) {
        return m_isPlaying;
    }
    
    PaError error = Pa_StartStream(m_stream);
    if (error != paNoError) {
        CF_LOG_ERROR("PortAudioEngine: Failed to start stream: %s", Pa_GetErrorText(error));
        emit errorOccurred(QString("Failed to start audio stream: %1").arg(Pa_GetErrorText(error)));
        return false;
    }
    
    m_isPlaying = true;
    CF_LOG_INFO("PortAudioEngine: Playback started");
    emit playbackStarted();
    return true;
}

void PortAudioEngine::stopPlayback()
{
    if (!m_isPlaying) {
        return;
    }
    
    m_shouldStop = true;
    
    if (m_stream && Pa_IsStreamActive(m_stream)) {
        PaError error = Pa_StopStream(m_stream);
        if (error != paNoError) {
            CF_LOG_ERROR("PortAudioEngine: Error stopping stream: %s", Pa_GetErrorText(error));
        }
    }
    
    m_isPlaying = false;
    CF_LOG_INFO("PortAudioEngine: Playback stopped");
    emit playbackStopped();
}

void PortAudioEngine::enqueueAudio(const QByteArray &audioData)
{
    if (audioData.isEmpty()) {
        return;
    }
    
    QMutexLocker locker(&m_queueMutex);
    m_audioQueue.enqueue(audioData);
    m_dataAvailable.wakeAll();
    
    CF_LOG_DEBUG("PortAudioEngine: Enqueued %d bytes of audio data", audioData.size());
}

void PortAudioEngine::clearQueue()
{
    QMutexLocker locker(&m_queueMutex);
    int clearedCount = m_audioQueue.size();
    m_audioQueue.clear();
    CF_LOG_INFO("PortAudioEngine: Cleared %d audio chunks from queue", clearedCount);
}

int PortAudioEngine::getQueueSize() const
{
    QMutexLocker locker(&m_queueMutex);
    return m_audioQueue.size();
}

QList<PortAudioEngine::AudioDevice> PortAudioEngine::enumerateDevices()
{
    QList<AudioDevice> devices;
    
    PaError error = Pa_Initialize();
    if (error != paNoError) {
        CF_LOG_ERROR("PortAudioEngine: Failed to initialize for device enumeration: %s", Pa_GetErrorText(error));
        return devices;
    }
    
    int numDevices = Pa_GetDeviceCount();
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxOutputChannels > 0) {
            AudioDevice device;
            device.deviceId = i;
            device.name = QString::fromUtf8(deviceInfo->name);
            device.maxInputChannels = deviceInfo->maxInputChannels;
            device.maxOutputChannels = deviceInfo->maxOutputChannels;
            device.defaultSampleRate = deviceInfo->defaultSampleRate;
            
            // 检查是否为WASAPI设备（Windows）
            const PaHostApiInfo *hostApiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
            device.isWASAPI = (hostApiInfo && QString::fromUtf8(hostApiInfo->name).contains("WASAPI"));
            
            devices.append(device);
        }
    }
    
    Pa_Terminate();
    return devices;
}

bool PortAudioEngine::setOutputDevice(int deviceId)
{
    if (m_isPlaying) {
        CF_LOG_ERROR("PortAudioEngine: Cannot change device while playing");
        return false;
    }
    
    if (m_initialized) {
        cleanupAudioStream();
    }
    
    m_outputDeviceId = deviceId;
    
    if (m_initialized) {
        return setupAudioStream();
    }
    
    return true;
}

int PortAudioEngine::audioCallback(const void *inputBuffer, void *outputBuffer,
                                 unsigned long framesPerBuffer,
                                 const PaStreamCallbackTimeInfo *timeInfo,
                                 PaStreamCallbackFlags statusFlags,
                                 void *userData)
{
    Q_UNUSED(inputBuffer)
    Q_UNUSED(timeInfo)
    Q_UNUSED(statusFlags)
    
    PortAudioEngine *engine = static_cast<PortAudioEngine*>(userData);
    if (engine) {
        engine->handleAudioCallback(outputBuffer, framesPerBuffer);
    }
    
    return paContinue;
}

void PortAudioEngine::handleAudioCallback(void *outputBuffer, unsigned long framesPerBuffer)
{
    int16_t *output = static_cast<int16_t*>(outputBuffer);
    int samplesNeeded = framesPerBuffer * m_channels;
    
    // 清空输出缓冲区
    memset(output, 0, samplesNeeded * sizeof(int16_t));
    
    QMutexLocker locker(&m_queueMutex);
    
    if (m_audioQueue.isEmpty()) {
        return; // 无数据时输出静音
    }
    
    // 累积音频数据，避免重复播放
    static QByteArray accumulatedData;
    
    // 如果累积数据不足，从队列中获取更多数据
    while (accumulatedData.size() < samplesNeeded * sizeof(int16_t) && !m_audioQueue.isEmpty()) {
        QByteArray audioData = m_audioQueue.dequeue();
        
        // 如果需要重采样
        if (m_needsResampling) {
            audioData = resampleAudio(audioData);
        }
        
        accumulatedData.append(audioData);
    }
    
    // 复制数据到输出缓冲区
    int samplesToCopy = qMin(samplesNeeded, static_cast<int>(accumulatedData.size() / sizeof(int16_t)));
    if (samplesToCopy > 0) {
        memcpy(output, accumulatedData.constData(), samplesToCopy * sizeof(int16_t));
        
        // 移除已播放的数据
        accumulatedData.remove(0, samplesToCopy * sizeof(int16_t));
    }
}

bool PortAudioEngine::initializeResampler(int inputRate, int outputRate)
{
    // 这里应该使用libsamplerate或其他重采样库
    // 为了简化，这里只是标记需要重采样
    CF_LOG_INFO("PortAudioEngine: Resampler needed: %d -> %d Hz", inputRate, outputRate);
    
    // TODO: 实现真正的重采样器
    // m_resampler = src_new(SRC_SINC_FASTEST, m_channels, &error);
    
    return true; // 临时返回true
}

QByteArray PortAudioEngine::resampleAudio(const QByteArray &inputData)
{
    // TODO: 实现真正的重采样
    // 这里暂时直接返回原始数据
    return inputData;
}

void PortAudioEngine::cleanupAudioStream()
{
    if (m_stream) {
        if (Pa_IsStreamActive(m_stream)) {
            Pa_StopStream(m_stream);
        }
        Pa_CloseStream(m_stream);
        m_stream = nullptr;
    }
    
    if (m_initialized) {
        Pa_Terminate();
        m_initialized = false;
    }
}

void PortAudioEngine::processAudioQueue()
{
    // 这个方法可以在单独的线程中处理音频队列
    // 目前音频处理主要在回调函数中进行
}
