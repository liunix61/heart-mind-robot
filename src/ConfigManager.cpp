#include "ConfigManager.h"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QApplication>

ConfigManager* ConfigManager::s_instance = nullptr;

// 默认配置结构，完全参照py-xiaozhi的DEFAULT_CONFIG
const QJsonObject ConfigManager::DEFAULT_CONFIG = []() {
    QJsonObject config;
    
    // SYSTEM_OPTIONS
    QJsonObject systemOptions;
    systemOptions["CLIENT_ID"] = QJsonValue::Null;
    systemOptions["DEVICE_ID"] = QJsonValue::Null;
    
    QJsonObject network;
    network["OTA_VERSION_URL"] = "https://api.tenclass.net/xiaozhi/ota/";
    network["WEBSOCKET_URL"] = QJsonValue::Null;
    network["WEBSOCKET_ACCESS_TOKEN"] = QJsonValue::Null;
    network["MQTT_INFO"] = QJsonValue::Null;
    network["ACTIVATION_VERSION"] = "v2";
    network["AUTHORIZATION_URL"] = "https://xiaozhi.me/";
    
    systemOptions["NETWORK"] = network;
    config["SYSTEM_OPTIONS"] = systemOptions;
    
    // WAKE_WORD_OPTIONS
    QJsonObject wakeWordOptions;
    wakeWordOptions["USE_WAKE_WORD"] = true;
    wakeWordOptions["MODEL_PATH"] = "models";
    wakeWordOptions["NUM_THREADS"] = 4;
    wakeWordOptions["PROVIDER"] = "cpu";
    wakeWordOptions["MAX_ACTIVE_PATHS"] = 2;
    wakeWordOptions["KEYWORDS_SCORE"] = 1.8;
    wakeWordOptions["KEYWORDS_THRESHOLD"] = 0.2;
    wakeWordOptions["NUM_TRAILING_BLANKS"] = 1;
    config["WAKE_WORD_OPTIONS"] = wakeWordOptions;
    
    // CAMERA
    QJsonObject camera;
    camera["camera_index"] = 0;
    camera["frame_width"] = 640;
    camera["frame_height"] = 480;
    camera["fps"] = 30;
    camera["Local_VL_url"] = "https://open.bigmodel.cn/api/paas/v4/";
    camera["VLapi_key"] = "";
    camera["models"] = "glm-4v-plus";
    config["CAMERA"] = camera;
    
    // SHORTCUTS
    QJsonObject shortcuts;
    shortcuts["ENABLED"] = true;
    shortcuts["MANUAL_PRESS"] = QJsonObject{{"modifier", "ctrl"}, {"key", "j"}, {"description", "按住说话"}};
    shortcuts["AUTO_TOGGLE"] = QJsonObject{{"modifier", "ctrl"}, {"key", "k"}, {"description", "自动对话"}};
    shortcuts["ABORT"] = QJsonObject{{"modifier", "ctrl"}, {"key", "q"}, {"description", "中断对话"}};
    shortcuts["MODE_TOGGLE"] = QJsonObject{{"modifier", "ctrl"}, {"key", "m"}, {"description", "切换模式"}};
    shortcuts["WINDOW_TOGGLE"] = QJsonObject{{"modifier", "ctrl"}, {"key", "w"}, {"description", "显示/隐藏窗口"}};
    config["SHORTCUTS"] = shortcuts;
    
    // AEC_OPTIONS
    QJsonObject aecOptions;
    aecOptions["ENABLED"] = false;
    aecOptions["BUFFER_MAX_LENGTH"] = 200;
    aecOptions["FRAME_DELAY"] = 3;
    aecOptions["FILTER_LENGTH_RATIO"] = 0.4;
    aecOptions["ENABLE_PREPROCESS"] = true;
    config["AEC_OPTIONS"] = aecOptions;
    
    // AUDIO_DEVICES
    QJsonObject audioDevices;
    audioDevices["input_device_id"] = QJsonValue::Null;
    audioDevices["input_device_name"] = QJsonValue::Null;
    audioDevices["output_device_id"] = QJsonValue::Null;
    audioDevices["output_device_name"] = QJsonValue::Null;
    audioDevices["input_sample_rate"] = QJsonValue::Null;
    audioDevices["output_sample_rate"] = QJsonValue::Null;
    config["AUDIO_DEVICES"] = audioDevices;
    
    return config;
}();

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
{
    if (m_initialized) return;
    m_initialized = true;

    initFilePaths();
    ensureRequiredDirectories();
    loadConfig();
    
    qDebug() << "ConfigManager initialized";
}

ConfigManager::~ConfigManager()
{
    // No explicit cleanup needed for QObject children
}

ConfigManager* ConfigManager::getInstance()
{
    if (!s_instance) {
        s_instance = new ConfigManager();
    }
    return s_instance;
}

void ConfigManager::initFilePaths()
{
    m_configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!m_configDir.exists()) {
        m_configDir.mkpath(".");
    }
    m_configFilePath = m_configDir.filePath("config.json");
    qDebug() << "ConfigManager config directory:" << m_configDir.path();
}

void ConfigManager::ensureRequiredDirectories()
{
    // 创建必要的目录结构
    QDir projectRoot = QDir::current();
    
    // 创建 models 目录
    QDir modelsDir = projectRoot;
    if (!modelsDir.cd("models")) {
        modelsDir.mkpath("models");
        qDebug() << "Created models directory:" << modelsDir.absoluteFilePath("models");
    }
    
    // 创建 cache 目录
    QDir cacheDir = projectRoot;
    if (!cacheDir.cd("cache")) {
        cacheDir.mkpath("cache");
        qDebug() << "Created cache directory:" << cacheDir.absoluteFilePath("cache");
    }
}

QJsonObject ConfigManager::loadConfigFromFile() const
{
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Config file does not exist or cannot be opened:" << m_configFilePath;
        return QJsonObject();
    }
    
    QByteArray jsonData = file.readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse config file:" << parseError.errorString();
        return QJsonObject();
    }
    
    return doc.object();
}

QJsonObject ConfigManager::mergeConfigs(const QJsonObject& defaultConfig, const QJsonObject& customConfig) const
{
    QJsonObject result = defaultConfig;
    
    for (auto it = customConfig.begin(); it != customConfig.end(); ++it) {
        const QString& key = it.key();
        const QJsonValue& value = it.value();
        
        if (result.contains(key) && result[key].isObject() && value.isObject()) {
            result[key] = mergeConfigs(result[key].toObject(), value.toObject());
        } else {
            result[key] = value;
        }
    }
    
    return result;
}

QString ConfigManager::generateUuid() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool ConfigManager::loadConfig()
{
    try {
        QJsonObject fileConfig = loadConfigFromFile();
        
        if (!fileConfig.isEmpty()) {
            qDebug() << "Config file found, merging with defaults";
            m_config = mergeConfigs(DEFAULT_CONFIG, fileConfig);
        } else {
            qDebug() << "Config file not found or empty, using defaults";
            m_config = DEFAULT_CONFIG;
            saveConfig(); // 保存默认配置
        }
        
        qDebug() << "Config loaded from:" << m_configFilePath;
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "Config loading error:" << e.what();
        m_config = DEFAULT_CONFIG;
        return false;
    }
}

bool ConfigManager::saveConfig()
{
    try {
        // 确保配置目录存在
        if (!m_configDir.exists()) {
            m_configDir.mkpath(".");
        }
        
        QFile file(m_configFilePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "Could not open config file for writing:" << file.errorString();
            return false;
        }
        
        QJsonDocument doc(m_config);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        
        qDebug() << "Config saved to:" << m_configFilePath;
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "Config saving error:" << e.what();
        return false;
    }
}

QVariant ConfigManager::getConfig(const QString& path, const QVariant& defaultValue) const
{
    try {
        QJsonValue value = m_config;
        QStringList keys = path.split('.');
        
        for (const QString& key : keys) {
            if (value.isObject()) {
                value = value.toObject()[key];
            } else {
                return defaultValue;
            }
        }
        
        return value.toVariant();
    } catch (const std::exception& e) {
        qWarning() << "Error getting config for path" << path << ":" << e.what();
        return defaultValue;
    }
}

void ConfigManager::updateNestedObject(QJsonObject& obj, const QStringList& parts, const QString& lastKey, const QVariant& value)
{
    if (parts.isEmpty()) {
        obj[lastKey] = QJsonValue::fromVariant(value);
        return;
    }
    
    QString currentPart = parts.first();
    QStringList remainingParts = parts.mid(1);
    
    if (!obj.contains(currentPart) || !obj[currentPart].isObject()) {
        obj[currentPart] = QJsonObject();
    }
    
    QJsonObject nested = obj[currentPart].toObject();
    updateNestedObject(nested, remainingParts, lastKey, value);
    obj[currentPart] = nested;
}

bool ConfigManager::updateConfig(const QString& path, const QVariant& value)
{
    try {
        QStringList parts = path.split('.');
        QString last = parts.takeLast();
        
        updateNestedObject(m_config, parts, last, value);
        
        return saveConfig();
    } catch (const std::exception& e) {
        qWarning() << "Error updating config for path" << path << ":" << e.what();
        return false;
    }
}

bool ConfigManager::reloadConfig()
{
    try {
        m_config = loadConfigFromFile();
        if (m_config.isEmpty()) {
            m_config = DEFAULT_CONFIG;
        }
        qDebug() << "Config reloaded successfully";
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Config reload failed:" << e.what();
        return false;
    }
}

void ConfigManager::initializeClientId()
{
    QString clientId = getConfig("SYSTEM_OPTIONS.CLIENT_ID").toString();
    if (clientId.isEmpty()) {
        clientId = generateUuid();
        if (updateConfig("SYSTEM_OPTIONS.CLIENT_ID", clientId)) {
            qDebug() << "Generated new client ID:" << clientId;
        } else {
            qWarning() << "Failed to save new client ID";
        }
    }
}

void ConfigManager::initializeDeviceIdFromFingerprint(const QString& macAddress)
{
    QString deviceId = getConfig("SYSTEM_OPTIONS.DEVICE_ID").toString();
    if (deviceId.isEmpty() && !macAddress.isEmpty()) {
        if (updateConfig("SYSTEM_OPTIONS.DEVICE_ID", macAddress)) {
            qDebug() << "Set device ID from MAC address:" << macAddress;
        } else {
            qWarning() << "Failed to save device ID";
        }
    }
}
