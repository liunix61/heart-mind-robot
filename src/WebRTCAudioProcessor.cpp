#include "WebRTCAudioProcessor.hpp"
#include <QDebug>
#include <QFile>
#include <QCoreApplication>
#include <cstring>

#ifdef __APPLE__
#include <dlfcn.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace webrtc_apm {

// WebRTC Config结构体（与Python版本对应）
#pragma pack(push, 1)
struct Pipeline {
    int maximum_internal_processing_rate;
    bool multi_channel_render;
    bool multi_channel_capture;
    int capture_downmix_method;
};

struct PreAmplifier {
    bool enabled;
    float fixed_gain_factor;
};

struct AnalogMicGainEmulation {
    bool enabled;
    int initial_level;
};

struct CaptureLevelAdjustment {
    bool enabled;
    float pre_gain_factor;
    float post_gain_factor;
    AnalogMicGainEmulation mic_gain_emulation;
};

struct HighPassFilter {
    bool enabled;
    bool apply_in_full_band;
};

struct EchoCanceller {
    bool enabled;
    bool mobile_mode;
    bool export_linear_aec_output;
    bool enforce_high_pass_filtering;
};

struct NoiseSuppression {
    bool enabled;
    int noise_level;
    bool analyze_linear_aec_output_when_available;
};

struct TransientSuppression {
    bool enabled;
};

struct ClippingPredictor {
    bool enabled;
    int predictor_mode;
    int window_length;
    int reference_window_length;
    int reference_window_delay;
    float clipping_threshold;
    float crest_factor_margin;
    bool use_predicted_step;
};

struct AnalogGainController {
    bool enabled;
    int startup_min_volume;
    int clipped_level_min;
    bool enable_digital_adaptive;
    int clipped_level_step;
    float clipped_ratio_threshold;
    int clipped_wait_frames;
    ClippingPredictor predictor;
};

struct GainController1 {
    bool enabled;
    int controller_mode;
    int target_level_dbfs;
    int compression_gain_db;
    bool enable_limiter;
    AnalogGainController analog_controller;
};

struct InputVolumeController {
    bool enabled;
};

struct AdaptiveDigital {
    bool enabled;
    float headroom_db;
    float max_gain_db;
    float initial_gain_db;
    float max_gain_change_db_per_second;
    float max_output_noise_level_dbfs;
};

struct FixedDigital {
    float gain_db;
};

struct GainController2 {
    bool enabled;
    InputVolumeController volume_controller;
    AdaptiveDigital adaptive_controller;
    FixedDigital fixed_controller;
};

struct WebRTCConfig {
    Pipeline pipeline_config;
    PreAmplifier pre_amp;
    CaptureLevelAdjustment level_adjustment;
    HighPassFilter high_pass;
    EchoCanceller echo;
    NoiseSuppression noise_suppress;
    TransientSuppression transient_suppress;
    GainController1 gain_control1;
    GainController2 gain_control2;
};
#pragma pack(pop)

WebRTCAudioProcessor::WebRTCAudioProcessor()
    : m_apmHandle(nullptr)
    , m_captureConfig(nullptr)
    , m_renderConfig(nullptr)
    , m_sampleRate(16000)
    , m_channels(1)
    , m_webrtcFrameSize(160)
    , m_initialized(false)
    , m_libraryHandle(nullptr)
    , m_createAPM(nullptr)
    , m_destroyAPM(nullptr)
    , m_createStreamConfig(nullptr)
    , m_destroyStreamConfig(nullptr)
    , m_applyConfig(nullptr)
    , m_processReverseStream(nullptr)
    , m_processStream(nullptr)
    , m_setStreamDelayMs(nullptr)
{
}

WebRTCAudioProcessor::~WebRTCAudioProcessor() {
    if (m_captureConfig && m_destroyStreamConfig) {
        m_destroyStreamConfig(m_captureConfig);
        m_captureConfig = nullptr;
    }
    
    if (m_renderConfig && m_destroyStreamConfig) {
        m_destroyStreamConfig(m_renderConfig);
        m_renderConfig = nullptr;
    }
    
    if (m_apmHandle && m_destroyAPM) {
        m_destroyAPM(m_apmHandle);
        m_apmHandle = nullptr;
    }
    
    unloadLibrary();
}

std::string WebRTCAudioProcessor::getPlatformName() {
#ifdef __APPLE__
    return "darwin";
#elif defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

bool WebRTCAudioProcessor::isPlatformSupported() {
    std::string platform = getPlatformName();
    return (platform == "darwin" || platform == "windows" || platform == "linux");
}

bool WebRTCAudioProcessor::loadLibrary() {
    if (m_libraryHandle) {
        return true; // 已经加载
    }
    
    // 构建库文件路径
    QString appDir = QCoreApplication::applicationDirPath();
    QString libPath;
    
#ifdef __APPLE__
    #if defined(__arm64__) || defined(__aarch64__)
        libPath = appDir + "/../third/webrtc_apm/macos/arm64/libwebrtc_apm.dylib";
    #else
        libPath = appDir + "/../third/webrtc_apm/macos/x64/libwebrtc_apm.dylib";
    #endif
#elif defined(_WIN32)
    #if defined(_M_ARM64) || defined(__aarch64__)
        libPath = appDir + "/../third/webrtc_apm/windows/arm64/libwebrtc_apm.dll";
    #elif defined(_M_X64) || defined(__x86_64__)
        libPath = appDir + "/../third/webrtc_apm/windows/x64/libwebrtc_apm.dll";
    #else
        libPath = appDir + "/../third/webrtc_apm/windows/x86/libwebrtc_apm.dll";
    #endif
#elif defined(__linux__)
    #if defined(__aarch64__) || defined(__arm64__)
        libPath = appDir + "/../third/webrtc_apm/linux/arm64/libwebrtc_apm.so";
    #else
        libPath = appDir + "/../third/webrtc_apm/linux/x64/libwebrtc_apm.so";
    #endif
#endif
    
    qDebug() << "尝试加载WebRTC库:" << libPath;
    
    if (!QFile::exists(libPath)) {
        qWarning() << "WebRTC库文件不存在:" << libPath;
        return false;
    }
    
#ifdef _WIN32
    m_libraryHandle = LoadLibraryW(libPath.toStdWString().c_str());
    if (!m_libraryHandle) {
        qWarning() << "无法加载WebRTC库 (错误码:" << GetLastError() << ")";
        return false;
    }
    
    #define LOAD_FUNCTION(funcName, varName) \
        varName = (decltype(varName))GetProcAddress((HMODULE)m_libraryHandle, "WebRTC_APM_" #funcName); \
        if (!varName) { \
            qWarning() << "无法加载函数: WebRTC_APM_" #funcName; \
            unloadLibrary(); \
            return false; \
        }
#else
    m_libraryHandle = dlopen(libPath.toUtf8().constData(), RTLD_LAZY);
    if (!m_libraryHandle) {
        qWarning() << "无法加载WebRTC库:" << dlerror();
        return false;
    }
    
    #define LOAD_FUNCTION(funcName, varName) \
        varName = (decltype(varName))dlsym(m_libraryHandle, "WebRTC_APM_" #funcName); \
        if (!varName) { \
            qWarning() << "无法加载函数: WebRTC_APM_" #funcName << "-" << dlerror(); \
            unloadLibrary(); \
            return false; \
        }
#endif
    
    // 加载所有函数
    LOAD_FUNCTION(Create, m_createAPM);
    LOAD_FUNCTION(Destroy, m_destroyAPM);
    LOAD_FUNCTION(CreateStreamConfig, m_createStreamConfig);
    LOAD_FUNCTION(DestroyStreamConfig, m_destroyStreamConfig);
    LOAD_FUNCTION(ApplyConfig, m_applyConfig);
    LOAD_FUNCTION(ProcessReverseStream, m_processReverseStream);
    LOAD_FUNCTION(ProcessStream, m_processStream);
    LOAD_FUNCTION(SetStreamDelayMs, m_setStreamDelayMs);
    
#undef LOAD_FUNCTION
    
    qDebug() << "WebRTC库加载成功";
    return true;
}

void WebRTCAudioProcessor::unloadLibrary() {
    if (m_libraryHandle) {
#ifdef _WIN32
        FreeLibrary((HMODULE)m_libraryHandle);
#else
        dlclose(m_libraryHandle);
#endif
        m_libraryHandle = nullptr;
    }
    
    m_createAPM = nullptr;
    m_destroyAPM = nullptr;
    m_createStreamConfig = nullptr;
    m_destroyStreamConfig = nullptr;
    m_applyConfig = nullptr;
    m_processReverseStream = nullptr;
    m_processStream = nullptr;
    m_setStreamDelayMs = nullptr;
}

bool WebRTCAudioProcessor::initialize(int sampleRate, int channels) {
    if (m_initialized) {
        qWarning() << "WebRTCAudioProcessor已经初始化";
        return true;
    }
    
    if (!isPlatformSupported()) {
        qWarning() << "当前平台不支持WebRTC AEC:" << QString::fromStdString(getPlatformName());
        return false;
    }
    
    // 加载动态库
    if (!loadLibrary()) {
        qWarning() << "无法加载WebRTC库";
        return false;
    }
    
    m_sampleRate = sampleRate;
    m_channels = channels;
    
    // 计算WebRTC帧大小（10ms）
    m_webrtcFrameSize = (sampleRate * 10) / 1000; // 10ms
    
    // 创建APM实例
    m_apmHandle = m_createAPM();
    if (!m_apmHandle) {
        qWarning() << "无法创建WebRTC APM实例";
        return false;
    }
    
    // 创建流配置
    m_captureConfig = m_createStreamConfig(sampleRate, channels);
    m_renderConfig = m_createStreamConfig(sampleRate, channels);
    
    if (!m_captureConfig || !m_renderConfig) {
        qWarning() << "无法创建流配置";
        if (m_apmHandle) m_destroyAPM(m_apmHandle);
        m_apmHandle = nullptr;
        return false;
    }
    
    m_initialized = true;
    qDebug() << "WebRTCAudioProcessor初始化成功 - 采样率:" << sampleRate 
             << "声道:" << channels << "帧大小:" << m_webrtcFrameSize;
    
    return true;
}

bool WebRTCAudioProcessor::applyConfig(const AudioProcessorConfig& config) {
    if (!m_initialized || !m_apmHandle || !m_applyConfig) {
        qWarning() << "WebRTCAudioProcessor未初始化";
        return false;
    }
    
    // 创建WebRTC配置结构
    WebRTCConfig webrtcConfig;
    memset(&webrtcConfig, 0, sizeof(WebRTCConfig));
    
    // 管道配置
    webrtcConfig.pipeline_config.maximum_internal_processing_rate = 48000;
    webrtcConfig.pipeline_config.multi_channel_render = false;
    webrtcConfig.pipeline_config.multi_channel_capture = false;
    webrtcConfig.pipeline_config.capture_downmix_method = 0; // AVERAGE_CHANNELS
    
    // 前置放大器
    webrtcConfig.pre_amp.enabled = false;
    webrtcConfig.pre_amp.fixed_gain_factor = 1.0f;
    
    // 电平调整
    webrtcConfig.level_adjustment.enabled = false;
    webrtcConfig.level_adjustment.pre_gain_factor = 1.0f;
    webrtcConfig.level_adjustment.post_gain_factor = 1.0f;
    webrtcConfig.level_adjustment.mic_gain_emulation.enabled = false;
    webrtcConfig.level_adjustment.mic_gain_emulation.initial_level = 255;
    
    // 高通滤波器
    webrtcConfig.high_pass.enabled = config.highPassFilterEnabled;
    webrtcConfig.high_pass.apply_in_full_band = config.highPassApplyInFullBand;
    
    // 回声消除
    webrtcConfig.echo.enabled = config.echoEnabled;
    webrtcConfig.echo.mobile_mode = config.echoMobileMode;
    webrtcConfig.echo.export_linear_aec_output = false;
    webrtcConfig.echo.enforce_high_pass_filtering = config.echoEnforceHighPassFiltering;
    
    // 噪声抑制
    webrtcConfig.noise_suppress.enabled = config.noiseSuppressionEnabled;
    webrtcConfig.noise_suppress.noise_level = static_cast<int>(config.noiseLevel);
    webrtcConfig.noise_suppress.analyze_linear_aec_output_when_available = false;
    
    // 瞬态抑制
    webrtcConfig.transient_suppress.enabled = false;
    
    // AGC1
    webrtcConfig.gain_control1.enabled = config.gainControl1Enabled;
    webrtcConfig.gain_control1.controller_mode = static_cast<int>(config.gainControlMode);
    webrtcConfig.gain_control1.target_level_dbfs = config.targetLevelDbfs;
    webrtcConfig.gain_control1.compression_gain_db = config.compressionGainDb;
    webrtcConfig.gain_control1.enable_limiter = config.enableLimiter;
    
    // AGC1 模拟控制器
    webrtcConfig.gain_control1.analog_controller.enabled = true;
    webrtcConfig.gain_control1.analog_controller.startup_min_volume = 0;
    webrtcConfig.gain_control1.analog_controller.clipped_level_min = 70;
    webrtcConfig.gain_control1.analog_controller.enable_digital_adaptive = true;
    webrtcConfig.gain_control1.analog_controller.clipped_level_step = 15;
    webrtcConfig.gain_control1.analog_controller.clipped_ratio_threshold = 0.1f;
    webrtcConfig.gain_control1.analog_controller.clipped_wait_frames = 300;
    
    // 削波预测器
    webrtcConfig.gain_control1.analog_controller.predictor.enabled = false;
    webrtcConfig.gain_control1.analog_controller.predictor.predictor_mode = 0;
    webrtcConfig.gain_control1.analog_controller.predictor.window_length = 5;
    webrtcConfig.gain_control1.analog_controller.predictor.reference_window_length = 5;
    webrtcConfig.gain_control1.analog_controller.predictor.reference_window_delay = 5;
    webrtcConfig.gain_control1.analog_controller.predictor.clipping_threshold = -1.0f;
    webrtcConfig.gain_control1.analog_controller.predictor.crest_factor_margin = 3.0f;
    webrtcConfig.gain_control1.analog_controller.predictor.use_predicted_step = true;
    
    // AGC2
    webrtcConfig.gain_control2.enabled = false;
    webrtcConfig.gain_control2.volume_controller.enabled = false;
    webrtcConfig.gain_control2.adaptive_controller.enabled = false;
    webrtcConfig.gain_control2.adaptive_controller.headroom_db = 5.0f;
    webrtcConfig.gain_control2.adaptive_controller.max_gain_db = 50.0f;
    webrtcConfig.gain_control2.adaptive_controller.initial_gain_db = 15.0f;
    webrtcConfig.gain_control2.adaptive_controller.max_gain_change_db_per_second = 6.0f;
    webrtcConfig.gain_control2.adaptive_controller.max_output_noise_level_dbfs = -50.0f;
    webrtcConfig.gain_control2.fixed_controller.gain_db = 0.0f;
    
    // 应用配置
    int result = m_applyConfig(m_apmHandle, &webrtcConfig);
    if (result != 0) {
        qWarning() << "WebRTC配置应用失败，错误码:" << result;
        return false;
    }
    
    qDebug() << "WebRTC配置应用成功 - AEC:" << config.echoEnabled 
             << "NS:" << config.noiseSuppressionEnabled
             << "HighPass:" << config.highPassFilterEnabled;
    
    return true;
}

void WebRTCAudioProcessor::setStreamDelayMs(int delayMs) {
    if (m_initialized && m_apmHandle && m_setStreamDelayMs) {
        m_setStreamDelayMs(m_apmHandle, delayMs);
        qDebug() << "设置流延迟:" << delayMs << "ms";
    }
}

bool WebRTCAudioProcessor::processReverseStream(const int16_t* audioData, size_t frameSize, int16_t* outputData) {
    if (!m_initialized || !m_apmHandle || !m_processReverseStream) {
        return false;
    }
    
    if (frameSize != static_cast<size_t>(m_webrtcFrameSize)) {
        qWarning() << "帧大小不匹配，期望:" << m_webrtcFrameSize << "实际:" << frameSize;
        return false;
    }
    
    int result = m_processReverseStream(m_apmHandle, audioData, m_renderConfig, m_renderConfig, outputData);
    if (result != 0) {
        qWarning() << "处理参考流失败，错误码:" << result;
        return false;
    }
    
    return true;
}

bool WebRTCAudioProcessor::processStream(const int16_t* audioData, size_t frameSize, int16_t* outputData) {
    if (!m_initialized || !m_apmHandle || !m_processStream) {
        return false;
    }
    
    if (frameSize != static_cast<size_t>(m_webrtcFrameSize)) {
        qWarning() << "帧大小不匹配，期望:" << m_webrtcFrameSize << "实际:" << frameSize;
        return false;
    }
    
    int result = m_processStream(m_apmHandle, audioData, m_captureConfig, m_captureConfig, outputData);
    if (result != 0) {
        qWarning() << "处理采集流失败，错误码:" << result;
        return false;
    }
    
    return true;
}

} // namespace webrtc_apm

