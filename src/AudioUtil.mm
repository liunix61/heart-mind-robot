//
// Created by 高煜焜 on 2023/11/4.
//
#include "AudioUtil.h"
#import <AVFoundation/AVFoundation.h>

// 音频播放工作线程实现
AudioPlaybackThread::AudioPlaybackThread(QObject *parent)
    : QThread(parent), m_running(false), m_opusDecoder(nullptr)
{
    m_opusDecoder = new OpusDecoder();
    if (!m_opusDecoder->initialize(24000, 1)) {
        CF_LOG_ERROR("AudioPlaybackThread: Failed to initialize Opus decoder");
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
    }
}

AudioPlaybackThread::~AudioPlaybackThread() {
    stopPlayback();
    if (m_opusDecoder) {
        delete m_opusDecoder;
    }
}

void AudioPlaybackThread::enqueueAudio(const QByteArray &audioData) {
    QMutexLocker locker(&m_queueMutex);
    m_audioQueue.enqueue(audioData);
}

void AudioPlaybackThread::stopPlayback() {
    m_running = false;
    wait();
}

void AudioPlaybackThread::run() {
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

void AudioPlaybackThread::processAudioData(const QByteArray &audioData) {
    if (!m_opusDecoder || !m_opusDecoder->isInitialized()) {
        return;
    }
    
    // 解码Opus数据为PCM
    QByteArray pcmData = m_opusDecoder->decode(audioData);
    if (pcmData.isEmpty()) {
        return;
    }
    
    // 将PCM数据转换为WAV格式
    @autoreleasepool {
        int sampleRate = m_opusDecoder->getSampleRate();
        int channels = m_opusDecoder->getChannels();
        int dataSize = pcmData.size();
        int totalSize = dataSize + 44; // WAV头44字节
        
        NSMutableData *wavData = [NSMutableData dataWithLength:totalSize];
        unsigned char *wavBytes = (unsigned char *)[wavData mutableBytes];
        
        // WAV文件头
        memcpy(wavBytes, "RIFF", 4);
        *((uint32_t*)(wavBytes + 4)) = totalSize - 8;
        memcpy(wavBytes + 8, "WAVE", 4);
        memcpy(wavBytes + 12, "fmt ", 4);
        *((uint32_t*)(wavBytes + 16)) = 16;
        *((uint16_t*)(wavBytes + 20)) = 1;  // PCM
        *((uint16_t*)(wavBytes + 22)) = channels;
        *((uint32_t*)(wavBytes + 24)) = sampleRate;
        *((uint32_t*)(wavBytes + 28)) = sampleRate * channels * 2;
        *((uint16_t*)(wavBytes + 32)) = channels * 2;
        *((uint16_t*)(wavBytes + 34)) = 16;
        memcpy(wavBytes + 36, "data", 4);
        *((uint32_t*)(wavBytes + 40)) = dataSize;
        
        // 复制PCM数据
        memcpy(wavBytes + 44, pcmData.constData(), dataSize);
        
        // 播放WAV数据
        NSError *error = nil;
        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithData:wavData error:&error];
        if (player) {
            [player prepareToPlay];
            [player play];
            
            // 等待播放完成
            while (player.isPlaying && m_running) {
                QThread::msleep(10);
            }
            
            [player stop];
        }
    }
}

AudioPlayer::AudioPlayer() 
    : m_opusDecoder(nullptr)
    , m_playbackThread(nullptr)
    , audioPlayer(nil) 
{
    // 初始化Opus解码器
    m_opusDecoder = new OpusDecoder();
    if (!m_opusDecoder->initialize(24000, 1)) {
        CF_LOG_ERROR("Failed to initialize Opus decoder");
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
    }
    
    // 创建并启动音频播放线程
    m_playbackThread = new AudioPlaybackThread();
    m_playbackThread->start();
}

AudioPlayer::~AudioPlayer() {
    // 停止播放线程
    if (m_playbackThread) {
        m_playbackThread->stopPlayback();
        delete m_playbackThread;
        m_playbackThread = nullptr;
    }
    
    if (audioPlayer) {
        [(AVAudioPlayer*)audioPlayer stop];
        audioPlayer = nil;
    }
    
    // 清理Opus解码器
    if (m_opusDecoder) {
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
    }
}

void AudioPlayer::playAudio(const char *audioData, size_t dataSize) {
    NSData *data = [NSData dataWithBytes:audioData length:dataSize];
    NSError *error = nil;

    AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithData:data error:&error];
    if (player) {
        audioPlayer = player;
        [player prepareToPlay];
        [player play];
    } else {
        CF_LOG_ERROR("初始化音频播放器时出错: %s", [[error localizedDescription] UTF8String]);
    }
    CF_LOG_DEBUG("play tts audio");
}

void AudioPlayer::playAudio(const char *filePath) {
    NSString *path = [NSString stringWithUTF8String:filePath];
    NSURL *url = [NSURL fileURLWithPath:path];

    NSError *error = nil;
    AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
    if (player) {
        audioPlayer = player;
        [player prepareToPlay];
        [player play];
    } else {
        CF_LOG_ERROR("初始化音频播放器时出错: %s", [[error localizedDescription] UTF8String]);
    }
    CF_LOG_DEBUG("play local audio");
}

void AudioPlayer::playReceivedAudioData(const QByteArray &audioData) {
    if (audioData.isEmpty()) {
        return;
    }
    
    // 将音频数据加入队列，由工作线程异步处理
    if (m_playbackThread) {
        m_playbackThread->enqueueAudio(audioData);
    }
    
    // 旧的同步实现已被异步线程替代
    /*
    // 检查Opus解码器是否可用
    if (!m_opusDecoder || !m_opusDecoder->isInitialized()) {
        CF_LOG_ERROR("Opus decoder not available, cannot play audio");
        return;
    }
    
    // 解码Opus数据为PCM
    QByteArray pcmData = m_opusDecoder->decode(audioData);
    // 这部分代码已被异步线程处理替代
    */
}
