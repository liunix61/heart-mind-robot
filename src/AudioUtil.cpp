#include "AudioUtil.h"
#include "Log_util.h"
#include "platform_config.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <mmreg.h>
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "winmm.lib")

// Windows音频播放引擎
class WindowsAudioEngine {
public:
    WindowsAudioEngine() : m_directSound(nullptr), m_primaryBuffer(nullptr), m_secondaryBuffer(nullptr), 
                          m_isInitialized(false), m_isPlaying(false), m_writePosition(0), m_bufferSize(0) {}
    
    ~WindowsAudioEngine() {
        cleanup();
    }
    
    bool initialize(int sampleRate, int channels) {
        HRESULT hr;
        
        // 创建DirectSound对象
        hr = DirectSoundCreate8(nullptr, &m_directSound, nullptr);
        if (FAILED(hr)) {
            CF_LOG_ERROR("Failed to create DirectSound: 0x%x", hr);
            return false;
        }
        
        // 设置协作级别
        hr = m_directSound->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);
        if (FAILED(hr)) {
            CF_LOG_ERROR("Failed to set cooperative level: 0x%x", hr);
            return false;
        }
        
        // 创建主缓冲区
        DSBUFFERDESC bufferDesc = {};
        bufferDesc.dwSize = sizeof(DSBUFFERDESC);
        bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
        
        hr = m_directSound->CreateSoundBuffer(&bufferDesc, &m_primaryBuffer, nullptr);
        if (FAILED(hr)) {
            CF_LOG_ERROR("Failed to create primary buffer: 0x%x", hr);
            return false;
        }
        
        // 设置主缓冲区格式
        WAVEFORMATEX waveFormat = {};
        waveFormat.wFormatTag = WAVE_FORMAT_PCM;
        waveFormat.nChannels = channels;
        waveFormat.nSamplesPerSec = sampleRate;
        waveFormat.wBitsPerSample = 16;
        waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        
        hr = m_primaryBuffer->SetFormat(&waveFormat);
        if (FAILED(hr)) {
            CF_LOG_ERROR("Failed to set primary buffer format: 0x%x", hr);
            return false;
        }
        
        // 创建流式播放的辅助缓冲区（较大的缓冲区用于流式播放）
        m_sampleRate = sampleRate;
        m_channels = channels;
        m_bufferSize = sampleRate * channels * 2 * 2; // 2秒的缓冲区
        
        WAVEFORMATEX streamFormat = {};
        streamFormat.wFormatTag = WAVE_FORMAT_PCM;
        streamFormat.nChannels = channels;
        streamFormat.nSamplesPerSec = sampleRate;
        streamFormat.wBitsPerSample = 16;
        streamFormat.nBlockAlign = streamFormat.nChannels * streamFormat.wBitsPerSample / 8;
        streamFormat.nAvgBytesPerSec = streamFormat.nSamplesPerSec * streamFormat.nBlockAlign;
        
        DSBUFFERDESC streamBufferDesc = {};
        streamBufferDesc.dwSize = sizeof(DSBUFFERDESC);
        streamBufferDesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        streamBufferDesc.dwBufferBytes = m_bufferSize;
        streamBufferDesc.lpwfxFormat = &streamFormat;
        
        hr = m_directSound->CreateSoundBuffer(&streamBufferDesc, &m_secondaryBuffer, nullptr);
        if (FAILED(hr)) {
            CF_LOG_ERROR("Failed to create secondary buffer: 0x%x", hr);
            return false;
        }
        
        m_isInitialized = true;
        CF_LOG_INFO("Windows audio engine initialized for streaming (sample rate: %d, channels: %d, buffer size: %d)", 
                   sampleRate, channels, m_bufferSize);
        return true;
    }
    
    void playPCMData(const QByteArray &pcmData) {
        if (!m_isInitialized || pcmData.isEmpty()) {
            CF_LOG_ERROR("WindowsAudioEngine: Not initialized or empty PCM data");
            return;
        }
        
        CF_LOG_INFO("WindowsAudioEngine: Streaming %d bytes of PCM data", pcmData.size());
        
        // 写入音频数据到流式缓冲区
        LPVOID audioPtr1, audioPtr2;
        DWORD audioBytes1, audioBytes2;
        
        HRESULT hr = m_secondaryBuffer->Lock(m_writePosition, pcmData.size(), 
                                            &audioPtr1, &audioBytes1, 
                                            &audioPtr2, &audioBytes2, 0);
        if (SUCCEEDED(hr)) {
            memcpy(audioPtr1, pcmData.constData(), audioBytes1);
            if (audioPtr2 != nullptr) {
                memcpy(audioPtr2, pcmData.constData() + audioBytes1, audioBytes2);
            }
            m_secondaryBuffer->Unlock(audioPtr1, audioBytes1, audioPtr2, audioBytes2);
            
            // 更新写入位置
            m_writePosition = (m_writePosition + pcmData.size()) % m_bufferSize;
            
            // 如果还没有开始播放，开始播放
            if (!m_isPlaying) {
                hr = m_secondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
                if (SUCCEEDED(hr)) {
                    m_isPlaying = true;
                    CF_LOG_INFO("WindowsAudioEngine: Started streaming playback");
                } else {
                    CF_LOG_ERROR("Failed to start streaming playback: 0x%x", hr);
                }
            }
        } else {
            CF_LOG_ERROR("Failed to lock streaming buffer: 0x%x", hr);
        }
    }
    
    void stopPlayback() {
        if (m_secondaryBuffer && m_isPlaying) {
            m_secondaryBuffer->Stop();
            m_isPlaying = false;
            m_writePosition = 0;
            CF_LOG_INFO("WindowsAudioEngine: Stopped streaming playback");
        }
    }
    
    void cleanup() {
        stopPlayback();
        if (m_secondaryBuffer) {
            m_secondaryBuffer->Release();
            m_secondaryBuffer = nullptr;
        }
        if (m_primaryBuffer) {
            m_primaryBuffer->Release();
            m_primaryBuffer = nullptr;
        }
        if (m_directSound) {
            m_directSound->Release();
            m_directSound = nullptr;
        }
        m_isInitialized = false;
    }
    
private:
    LPDIRECTSOUND8 m_directSound;
    LPDIRECTSOUNDBUFFER m_primaryBuffer;
    LPDIRECTSOUNDBUFFER m_secondaryBuffer;
    bool m_isInitialized;
    bool m_isPlaying;
    DWORD m_writePosition;
    DWORD m_bufferSize;
    int m_sampleRate;
    int m_channels;
};
#endif

AudioPlayer::AudioPlayer(QObject *parent) 
    : QObject(parent), m_opusDecoder(nullptr)
{
    WINDOWS_SPECIFIC(
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        audioPlayer = new WindowsAudioEngine();
        if (!static_cast<WindowsAudioEngine*>(audioPlayer)->initialize(24000, 1)) {
            CF_LOG_ERROR("Failed to initialize Windows audio engine");
            delete static_cast<WindowsAudioEngine*>(audioPlayer);
            audioPlayer = nullptr;
        }
    )
    
    // 创建并启动音频播放线程（包含Opus解码器）
    m_playbackThread = new AudioPlaybackThread(this);
    m_playbackThread->start();
    
    // 连接信号，转发解码后的音频数据
    connect(m_playbackThread, &AudioPlaybackThread::audioDecoded,
            this, &AudioPlayer::audioDecoded);
    
    // 连接PCM数据播放信号
    connect(m_playbackThread, &AudioPlaybackThread::audioDecoded,
            this, &AudioPlayer::onPCMDataReady);
    
    CF_LOG_INFO("AudioPlayer initialized");
}

AudioPlayer::~AudioPlayer() {
    // 停止播放线程
    if (m_playbackThread) {
        m_playbackThread->stopPlayback();
        delete m_playbackThread;
        m_playbackThread = nullptr;
    }
    
    WINDOWS_SPECIFIC(
        if (audioPlayer) {
            static_cast<WindowsAudioEngine*>(audioPlayer)->cleanup();
            delete static_cast<WindowsAudioEngine*>(audioPlayer);
            audioPlayer = nullptr;
        }
        CoUninitialize();
    )
    
    CF_LOG_INFO("AudioPlayer destroyed");
}

void AudioPlayer::playAudio(const char *filePath) {
    WINDOWS_SPECIFIC(
        // Windows 实现
        std::wstring wFilePath = std::wstring(filePath, filePath + strlen(filePath));
        PlaySoundW(wFilePath.c_str(), NULL, SND_FILENAME | SND_ASYNC);
    )
}

void AudioPlayer::playAudio(const char *audioData, size_t dataSize) {
    WINDOWS_SPECIFIC(
        // Windows 实现
        PlaySoundA((LPCSTR)audioData, NULL, SND_MEMORY | SND_ASYNC);
    )
}

void AudioPlayer::playReceivedAudioData(const QByteArray &audioData) {
    if (audioData.isEmpty()) {
        CF_LOG_INFO("Empty audio data received, skipping playback");
        return;
    }
    
    // 将Opus编码的音频数据加入队列，由工作线程异步解码并播放
    if (m_playbackThread) {
        m_playbackThread->enqueueAudio(audioData);
        CF_LOG_DEBUG("Enqueued audio data for playback, size: %d bytes", audioData.size());
    } else {
        CF_LOG_ERROR("Playback thread not available");
    }
}

// AudioPlaybackThread 实现
AudioPlaybackThread::AudioPlaybackThread(QObject *parent)
    : QThread(parent), m_running(false), m_opusDecoder(nullptr)
{
    // 初始化Opus解码器
    m_opusDecoder = new OpusDecoder();
    if (!m_opusDecoder->initialize(24000, 1)) {
        CF_LOG_ERROR("AudioPlaybackThread: Failed to initialize Opus decoder");
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
    }
}

AudioPlaybackThread::~AudioPlaybackThread()
{
    stopPlayback();
    if (m_opusDecoder) {
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
    }
}

void AudioPlaybackThread::enqueueAudio(const QByteArray &audioData)
{
    QMutexLocker locker(&m_queueMutex);
    m_audioQueue.enqueue(audioData);
}

void AudioPlaybackThread::stopPlayback()
{
    m_running = false;
    wait();
}

void AudioPlaybackThread::clearAudioQueue()
{
    CF_LOG_INFO("AudioPlaybackThread: Clearing audio queue");
    
    QMutexLocker locker(&m_queueMutex);
    int clearedCount = m_audioQueue.size();
    m_audioQueue.clear();
    CF_LOG_INFO("AudioPlaybackThread: Cleared %d queued audio chunks", clearedCount);
}

void AudioPlaybackThread::run()
{
    m_running = true;
    
    while (m_running) {
        QByteArray audioData;
        {
            QMutexLocker locker(&m_queueMutex);
            if (!m_audioQueue.isEmpty()) {
                audioData = m_audioQueue.dequeue();
            }
        }
        
        if (!audioData.isEmpty()) {
            processAudioData(audioData);
        } else {
            msleep(10); // 短暂休眠，避免CPU占用
        }
    }
}

void AudioPlaybackThread::processAudioData(const QByteArray &audioData)
{
    CF_LOG_INFO("AudioPlaybackThread: Processing %d bytes of Opus data", audioData.size());
    
    if (!m_opusDecoder || !m_opusDecoder->isInitialized()) {
        CF_LOG_ERROR("AudioPlaybackThread: Decoder not initialized");
        return;
    }
    
    // 解码Opus数据为PCM
    QByteArray pcmData = m_opusDecoder->decode(audioData);
    if (pcmData.isEmpty()) {
        CF_LOG_ERROR("AudioPlaybackThread: Failed to decode opus data");
        return;
    }
    
    CF_LOG_INFO("AudioPlaybackThread: Successfully decoded to %d bytes PCM", pcmData.size());
    
    // 播放PCM数据
    WINDOWS_SPECIFIC(
        // 这里需要访问AudioPlayer的WindowsAudioEngine
        // 由于线程限制，我们通过信号传递PCM数据给主线程播放
    )
    
    // 发射信号，用于口型同步和播放
    CF_LOG_INFO("========================================");
    CF_LOG_INFO("AudioPlaybackThread: EMITTING audioDecoded signal!");
    CF_LOG_INFO("PCM size: %d bytes", pcmData.size());
    CF_LOG_INFO("========================================");
    emit audioDecoded(pcmData);
    
    CF_LOG_DEBUG("AudioPlaybackThread: Audio processing completed");
}

void AudioPlayer::clearAudioQueue()
{
    CF_LOG_INFO("AudioPlayer: Clearing audio queue for interruption");
    
    // 清空播放线程的音频队列
    if (m_playbackThread) {
        m_playbackThread->clearAudioQueue();
        CF_LOG_INFO("AudioPlayer: Audio queue cleared successfully");
    } else {
        CF_LOG_ERROR("AudioPlayer: Playback thread not available");
    }
    
    // 停止当前的音频播放
    WINDOWS_SPECIFIC(
        if (audioPlayer) {
            static_cast<WindowsAudioEngine*>(audioPlayer)->stopPlayback();
        }
    )
}

// 处理解码后的PCM数据播放
void AudioPlayer::onPCMDataReady(const QByteArray &pcmData)
{
    WINDOWS_SPECIFIC(
        if (audioPlayer) {
            static_cast<WindowsAudioEngine*>(audioPlayer)->playPCMData(pcmData);
        }
    )
}