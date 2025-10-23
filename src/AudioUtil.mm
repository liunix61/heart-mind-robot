//
// Created by 高煜焜 on 2023/11/4.
//
#include "AudioUtil.h"
#import <AVFoundation/AVFoundation.h>

AudioPlayer::AudioPlayer() 
    : m_opusDecoder(nullptr)
    , audioPlayer(nil) 
{
    // 初始化Opus解码器
    m_opusDecoder = new OpusDecoder();
    if (!m_opusDecoder->initialize(24000, 1)) {
        CF_LOG_ERROR("Failed to initialize Opus decoder");
        delete m_opusDecoder;
        m_opusDecoder = nullptr;
    }
}

AudioPlayer::~AudioPlayer() {
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
        CF_LOG_INFO("Empty audio data received, skipping playback");
        return;
    }
    
    CF_LOG_DEBUG("Playing received audio data, size: %zu bytes", audioData.size());
    
    // 检查Opus解码器是否可用
    if (!m_opusDecoder || !m_opusDecoder->isInitialized()) {
        CF_LOG_ERROR("Opus decoder not available, cannot play audio");
        return;
    }
    
    // 解码Opus数据为PCM
    QByteArray pcmData = m_opusDecoder->decode(audioData);
    if (pcmData.isEmpty()) {
        CF_LOG_ERROR("Failed to decode Opus audio data");
        return;
    }
    
    CF_LOG_DEBUG("Opus decoded successfully, PCM size: %zu bytes", pcmData.size());
    
    // 将PCM数据转换为WAV格式，因为AVAudioPlayer需要完整的音频文件格式
    // 创建WAV文件头
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
    *((uint32_t*)(wavBytes + 16)) = 16; // fmt chunk size
    *((uint16_t*)(wavBytes + 20)) = 1;  // PCM format
    *((uint16_t*)(wavBytes + 22)) = channels;
    *((uint32_t*)(wavBytes + 24)) = sampleRate;
    *((uint32_t*)(wavBytes + 28)) = sampleRate * channels * 2; // byte rate
    *((uint16_t*)(wavBytes + 32)) = channels * 2; // block align
    *((uint16_t*)(wavBytes + 34)) = 16; // bits per sample
    memcpy(wavBytes + 36, "data", 4);
    *((uint32_t*)(wavBytes + 40)) = dataSize;
    
    // 复制PCM数据
    memcpy(wavBytes + 44, pcmData.constData(), dataSize);
    
    NSError *error = nil;
    AVAudioPlayer *player = [[AVAudioPlayer alloc] initWithData:wavData error:&error];
    if (player) {
        audioPlayer = player;
        [player prepareToPlay];
        [player play];
        CF_LOG_DEBUG("Successfully started playing decoded PCM audio data as WAV");
    } else {
        CF_LOG_ERROR("初始化WAV音频播放器时出错: %s", [[error localizedDescription] UTF8String]);
    }
}
