#ifndef WEBRTCAUDIOPROCESSOR_HPP
#define WEBRTCAUDIOPROCESSOR_HPP

#include <memory>
#include <vector>
#include <cstdint>
#include <string>

// WebRTC音频处理器的C++封装
// 提供回声消除(AEC)、噪声抑制(NS)等功能

// 前向声明
struct WebRTCAudioProcessingHandle;

namespace webrtc_apm {

// 枚举类型
enum class NoiseSuppressionLevel {
    LOW = 0,
    MODERATE = 1,
    HIGH = 2,
    VERY_HIGH = 3
};

enum class GainController1Mode {
    ADAPTIVE_ANALOG = 0,
    ADAPTIVE_DIGITAL = 1,
    FIXED_DIGITAL = 2
};

// 配置结构体（简化版，只包含常用配置）
struct AudioProcessorConfig {
    // 回声消除
    bool echoEnabled = false;
    bool echoMobileMode = false;
    bool echoEnforceHighPassFiltering = true;
    
    // 噪声抑制
    bool noiseSuppressionEnabled = false;
    NoiseSuppressionLevel noiseLevel = NoiseSuppressionLevel::HIGH;
    
    // 高通滤波器
    bool highPassFilterEnabled = false;
    bool highPassApplyInFullBand = true;
    
    // 增益控制
    bool gainControl1Enabled = false;
    GainController1Mode gainControlMode = GainController1Mode::ADAPTIVE_DIGITAL;
    int targetLevelDbfs = 3;
    int compressionGainDb = 9;
    bool enableLimiter = true;
};

class WebRTCAudioProcessor {
public:
    WebRTCAudioProcessor();
    ~WebRTCAudioProcessor();
    
    // 禁止拷贝
    WebRTCAudioProcessor(const WebRTCAudioProcessor&) = delete;
    WebRTCAudioProcessor& operator=(const WebRTCAudioProcessor&) = delete;
    
    // 初始化
    bool initialize(int sampleRate = 16000, int channels = 1);
    
    // 应用配置
    bool applyConfig(const AudioProcessorConfig& config);
    
    // 设置流延迟（毫秒）
    void setStreamDelayMs(int delayMs);
    
    // 处理参考信号（扬声器输出）
    bool processReverseStream(const int16_t* audioData, size_t frameSize, int16_t* outputData);
    
    // 处理采集信号（麦克风输入）- 应用AEC、NS等处理
    bool processStream(const int16_t* audioData, size_t frameSize, int16_t* outputData);
    
    // 获取是否已初始化
    bool isInitialized() const { return m_initialized; }
    
    // 获取采样率
    int getSampleRate() const { return m_sampleRate; }
    
    // 获取声道数
    int getChannels() const { return m_channels; }
    
    // 获取WebRTC标准帧大小（10ms）
    int getWebRTCFrameSize() const { return m_webrtcFrameSize; }
    
    // 检查当前平台是否支持WebRTC AEC
    static bool isPlatformSupported();
    
    // 获取平台信息
    static std::string getPlatformName();
    
private:
    void* m_apmHandle;              // WebRTC APM实例句柄
    void* m_captureConfig;          // 采集流配置
    void* m_renderConfig;           // 渲染流配置
    
    int m_sampleRate;               // 采样率
    int m_channels;                 // 声道数
    int m_webrtcFrameSize;          // WebRTC标准帧大小（10ms = 160 samples @ 16kHz）
    bool m_initialized;             // 是否已初始化
    
    // 加载动态库
    bool loadLibrary();
    void unloadLibrary();
    
    void* m_libraryHandle;          // 动态库句柄
    
    // 函数指针
    typedef void* (*CreateAPMFunc)();
    typedef void (*DestroyAPMFunc)(void*);
    typedef void* (*CreateStreamConfigFunc)(int, int);
    typedef void (*DestroyStreamConfigFunc)(void*);
    typedef int (*ApplyConfigFunc)(void*, void*);
    typedef int (*ProcessReverseStreamFunc)(void*, const int16_t*, void*, void*, int16_t*);
    typedef int (*ProcessStreamFunc)(void*, const int16_t*, void*, void*, int16_t*);
    typedef void (*SetStreamDelayMsFunc)(void*, int);
    
    CreateAPMFunc m_createAPM;
    DestroyAPMFunc m_destroyAPM;
    CreateStreamConfigFunc m_createStreamConfig;
    DestroyStreamConfigFunc m_destroyStreamConfig;
    ApplyConfigFunc m_applyConfig;
    ProcessReverseStreamFunc m_processReverseStream;
    ProcessStreamFunc m_processStream;
    SetStreamDelayMsFunc m_setStreamDelayMs;
};

} // namespace webrtc_apm

#endif // WEBRTCAUDIOPROCESSOR_HPP

