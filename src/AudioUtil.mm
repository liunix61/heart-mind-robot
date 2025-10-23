//
// Created by 高煜焜 on 2023/11/4.
//
#include "AudioUtil.h"
#import <AVFoundation/AVFoundation.h>

// Objective-C 类用于管理音频引擎
@interface AudioEngineManager : NSObject {
@public
    AVAudioEngine *audioEngine;
    AVAudioPlayerNode *playerNode;
    AVAudioFormat *audioFormat;
    NSMutableArray *pcmBufferQueue;
    NSLock *queueLock;
    BOOL isPlaying;
}
- (instancetype)initWithSampleRate:(double)sampleRate channels:(AVAudioChannelCount)channels;
- (void)enqueuePCMData:(NSData *)pcmData;
- (void)start;
- (void)stop;
- (void)cleanup;
@end

@implementation AudioEngineManager

- (instancetype)initWithSampleRate:(double)sampleRate channels:(AVAudioChannelCount)channels {
    self = [super init];
    if (self) {
        audioEngine = [[AVAudioEngine alloc] init];
        playerNode = [[AVAudioPlayerNode alloc] init];
        
        // 创建音频格式 (PCM, 32-bit float - AVAudioEngine标准格式)
        audioFormat = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                       sampleRate:sampleRate
                                                         channels:channels
                                                      interleaved:NO];  // 非交错格式
        
        pcmBufferQueue = [[NSMutableArray alloc] init];
        queueLock = [[NSLock alloc] init];
        isPlaying = NO;
        
        // 连接节点
        [audioEngine attachNode:playerNode];
        [audioEngine connect:playerNode to:audioEngine.mainMixerNode format:audioFormat];
        
        CF_LOG_INFO("AudioEngineManager initialized with sample rate: %.0f, channels: %u", sampleRate, channels);
    }
    return self;
}

- (void)enqueuePCMData:(NSData *)pcmData {
    if (!pcmData || pcmData.length == 0) {
        return;
    }
    
    @autoreleasepool {
        // 输入是16-bit PCM，需要转换为32-bit float
        const int16_t *int16Data = (const int16_t *)pcmData.bytes;
        AVAudioFrameCount frameCount = (AVAudioFrameCount)(pcmData.length / sizeof(int16_t));
        
        AVAudioPCMBuffer *buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:audioFormat frameCapacity:frameCount];
        buffer.frameLength = frameCount;
        
        // 转换 int16 to float32 并复制数据（范围：-1.0 to 1.0）
        float *floatChannelData = buffer.floatChannelData[0];
        for (AVAudioFrameCount i = 0; i < frameCount; i++) {
            floatChannelData[i] = int16Data[i] / 32768.0f;  // 归一化到 -1.0 ~ 1.0
        }
        
        // 加入队列
        [queueLock lock];
        [pcmBufferQueue addObject:buffer];
        [queueLock unlock];
        
        // 如果正在播放，立即调度buffer
        if (isPlaying) {
            [self scheduleNextBuffer];
        }
    }
}

- (void)scheduleNextBuffer {
    AVAudioPCMBuffer *buffer = nil;
    
    [queueLock lock];
    if (pcmBufferQueue.count > 0) {
        buffer = [pcmBufferQueue firstObject];
        [pcmBufferQueue removeObjectAtIndex:0];
    }
    [queueLock unlock];
    
    if (buffer) {
        // 调度buffer播放，并在完成时调度下一个
        // 使用unsafe_unretained在非ARC模式下
        __unsafe_unretained AudioEngineManager *blockSelf = self;
        [playerNode scheduleBuffer:buffer completionHandler:^{
            if (blockSelf && blockSelf->isPlaying) {
                [blockSelf scheduleNextBuffer];
            }
        }];
    }
}

- (void)start {
    if (isPlaying) {
        return;
    }
    
    NSError *error = nil;
    if (![audioEngine startAndReturnError:&error]) {
        CF_LOG_ERROR("Failed to start audio engine: %s", [[error localizedDescription] UTF8String]);
        return;
    }
    
    [playerNode play];
    isPlaying = YES;
    
    // 开始调度队列中的buffer
    [self scheduleNextBuffer];
    
    CF_LOG_INFO("Audio engine started");
}

- (void)stop {
    if (!isPlaying) {
        return;
    }
    
    isPlaying = NO;
    [playerNode stop];
    [audioEngine stop];
    
    // 清空队列
    [queueLock lock];
    [pcmBufferQueue removeAllObjects];
    [queueLock unlock];
    
    CF_LOG_INFO("Audio engine stopped");
}

- (void)cleanup {
    [self stop];
    audioEngine = nil;
    playerNode = nil;
    audioFormat = nil;
    [pcmBufferQueue removeAllObjects];
    pcmBufferQueue = nil;
    queueLock = nil;
}

@end

// 音频播放工作线程实现
AudioPlaybackThread::AudioPlaybackThread(QObject *parent)
    : QThread(parent), m_running(false), m_opusDecoder(nullptr), m_audioEngineManager(nullptr)
{
    m_opusDecoder = new OpusDecoder();
    if (!m_opusDecoder->initialize(24000, 1)) {
        CF_LOG_ERROR("AudioPlaybackThread: Failed to initialize Opus decoder");
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
        return;
    }
    
    // 创建音频引擎管理器
    @autoreleasepool {
        AudioEngineManager *manager = [[AudioEngineManager alloc] initWithSampleRate:24000.0 channels:1];
        m_audioEngineManager = (void*)manager;  // 手动管理内存
        [manager start];
    }
}

AudioPlaybackThread::~AudioPlaybackThread() {
    stopPlayback();
    
    if (m_audioEngineManager) {
        @autoreleasepool {
            AudioEngineManager *manager = (AudioEngineManager*)m_audioEngineManager;
            [manager cleanup];
            [manager release];  // 手动释放内存
            m_audioEngineManager = nullptr;
        }
    }
    
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
    if (!m_opusDecoder || !m_opusDecoder->isInitialized() || !m_audioEngineManager) {
        CF_LOG_ERROR("AudioPlaybackThread: Decoder or audio engine not initialized");
        return;
    }
    
    // 解码Opus数据为PCM
    QByteArray pcmData = m_opusDecoder->decode(audioData);
    if (pcmData.isEmpty()) {
        CF_LOG_ERROR("AudioPlaybackThread: Failed to decode opus data");
        return;
    }
    
    // 将PCM数据加入播放队列
    @autoreleasepool {
        NSData *nsData = [NSData dataWithBytes:pcmData.constData() length:pcmData.size()];
        AudioEngineManager *manager = (AudioEngineManager*)m_audioEngineManager;
        [manager enqueuePCMData:nsData];
    }
    
    CF_LOG_DEBUG("AudioPlaybackThread: Enqueued PCM data, size: %d bytes", pcmData.size());
}

AudioPlayer::AudioPlayer() 
    : m_playbackThread(nullptr)
    , audioPlayer(nil) 
{
    // 创建并启动音频播放线程（包含Opus解码器）
    m_playbackThread = new AudioPlaybackThread();
    m_playbackThread->start();
    CF_LOG_INFO("AudioPlayer initialized");
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
    
    CF_LOG_INFO("AudioPlayer destroyed");
}

void AudioPlayer::playAudio(const char *audioData, size_t dataSize) {
    @autoreleasepool {
        NSData *data = [NSData dataWithBytes:audioData length:dataSize];
        NSError *error = nil;

        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithData:data error:&error];
        if (player) {
            audioPlayer = player;
            [player prepareToPlay];
            [player play];
            CF_LOG_DEBUG("Playing local audio from memory");
        } else {
            CF_LOG_ERROR("初始化音频播放器时出错: %s", [[error localizedDescription] UTF8String]);
        }
    }
}

void AudioPlayer::playAudio(const char *filePath) {
    @autoreleasepool {
        NSString *path = [NSString stringWithUTF8String:filePath];
        NSURL *url = [NSURL fileURLWithPath:path];

        NSError *error = nil;
        AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:&error];
        if (player) {
            audioPlayer = player;
            [player prepareToPlay];
            [player play];
            CF_LOG_DEBUG("Playing local audio from file: %s", filePath);
        } else {
            CF_LOG_ERROR("初始化音频播放器时出错: %s", [[error localizedDescription] UTF8String]);
        }
    }
}

void AudioPlayer::playReceivedAudioData(const QByteArray &audioData) {
    if (audioData.isEmpty()) {
        CF_LOG_INFO("Received empty audio data, skipping");
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
