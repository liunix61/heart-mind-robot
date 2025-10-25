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
    , m_resampleRatio(1.0)
    , m_processingThread(nullptr)
    , m_shouldStop(false)
{
    CF_LOG_INFO("PortAudioEngine: Constructor called");
}

PortAudioEngine::~PortAudioEngine()
{
    stopPlayback();
    cleanupAudioStream();
    
    // 清理重采样缓冲区
    m_resampleBuffer.clear();
    
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
    
    // 使用成员变量和互斥锁，确保线程安全
    QMutexLocker accumulatedLocker(&m_accumulatedDataMutex);
    
    // 如果累积数据不足，从队列中获取更多数据（限制循环次数防止无限循环）
    int maxIterations = 10; // 最大循环次数
    int iterationCount = 0;
    
    while (m_accumulatedData.size() < samplesNeeded * sizeof(int16_t) && 
           !m_audioQueue.isEmpty() && 
           iterationCount < maxIterations) {
        
        QByteArray audioData = m_audioQueue.dequeue();
        
        // 如果需要重采样
        if (m_needsResampling) {
            QByteArray resampledData = resampleAudio(audioData);
            if (!resampledData.isEmpty()) {
                audioData = resampledData;
            } else {
                CF_LOG_ERROR("PortAudioEngine: Resampling failed, using original data");
                // 如果重采样失败，使用原始数据
            }
        }
        
        if (!audioData.isEmpty()) {
            m_accumulatedData.append(audioData);
        } else {
            CF_LOG_ERROR("PortAudioEngine: Empty audio data, skipping");
        }
        iterationCount++;
        
        CF_LOG_INFO("PortAudioEngine: Audio callback iteration %d, accumulated: %d bytes", 
                   iterationCount, m_accumulatedData.size());
    }
    
    if (iterationCount >= maxIterations) {
        CF_LOG_ERROR("PortAudioEngine: Audio callback reached max iterations, possible infinite loop!");
    }
    
    // 复制数据到输出缓冲区
    int samplesToCopy = qMin(samplesNeeded, static_cast<int>(m_accumulatedData.size() / sizeof(int16_t)));
    if (samplesToCopy > 0) {
        memcpy(output, m_accumulatedData.constData(), samplesToCopy * sizeof(int16_t));
        
        // 移除已播放的数据
        m_accumulatedData.remove(0, samplesToCopy * sizeof(int16_t));
    }
}

bool PortAudioEngine::initializeResampler(int inputRate, int outputRate)
{
    CF_LOG_INFO("PortAudioEngine: Initializing resampler: %d -> %d Hz", inputRate, outputRate);
    
    // 计算重采样比例
    m_resampleRatio = static_cast<double>(outputRate) / static_cast<double>(inputRate);
    CF_LOG_INFO("PortAudioEngine: Resample ratio: %.3f", m_resampleRatio);
    
    // 初始化重采样缓冲区
    m_resampleBuffer.clear();
    m_resampleBuffer.reserve(8192); // 预分配缓冲区
    
    return true;
}

QByteArray PortAudioEngine::resampleAudio(const QByteArray &inputData)
{
    if (!m_needsResampling || inputData.isEmpty()) {
        return inputData;
    }
    
    const int16_t *inputSamples = reinterpret_cast<const int16_t*>(inputData.constData());
    int inputSampleCount = inputData.size() / sizeof(int16_t);
    
    // 计算输出样本数量
    int outputSampleCount = static_cast<int>(inputSampleCount * m_resampleRatio);
    if (outputSampleCount <= 0 || inputSampleCount <= 0) {
        CF_LOG_ERROR("PortAudioEngine: Invalid sample count for resampling: input=%d, output=%d", inputSampleCount, outputSampleCount);
        return QByteArray();
    }
    
    QByteArray outputData;
    outputData.resize(outputSampleCount * sizeof(int16_t));
    int16_t *outputSamples = reinterpret_cast<int16_t*>(outputData.data());
    
    // 线性插值重采样
    for (int i = 0; i < outputSampleCount; i++) {
        double inputIndex = i / m_resampleRatio;
        int inputIndexInt = static_cast<int>(inputIndex);
        double fraction = inputIndex - inputIndexInt;
        
        if (inputIndexInt >= inputSampleCount - 1 || inputIndexInt < 0) {
            // 超出范围，使用最后一个样本或第一个样本
            if (inputIndexInt >= inputSampleCount - 1) {
                outputSamples[i] = inputSamples[inputSampleCount - 1];
            } else {
                outputSamples[i] = inputSamples[0];
            }
        } else {
            // 线性插值
            int16_t sample1 = inputSamples[inputIndexInt];
            int16_t sample2 = inputSamples[inputIndexInt + 1];
            outputSamples[i] = static_cast<int16_t>(sample1 + fraction * (sample2 - sample1));
        }
    }
    
    CF_LOG_INFO("PortAudioEngine: Resampled %d -> %d samples", inputSampleCount, outputSampleCount);
    return outputData;
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
