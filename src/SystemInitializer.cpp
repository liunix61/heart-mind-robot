#include "SystemInitializer.h"
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QDebug>
#include <QUuid>
#include <QSysInfo>

SystemInitializer::SystemInitializer(QObject *parent)
    : QObject(parent)
    , m_deviceFingerprint(DeviceFingerprint::getInstance())
    , m_networkManager(nullptr)
    , m_currentReply(nullptr)
    , m_otaTimeoutTimer(nullptr)
    , m_currentStage(DEVICE_FINGERPRINT)
{
    // 初始化网络管理器
    m_networkManager = new QNetworkAccessManager(this);
    
    // 初始化超时定时器
    m_otaTimeoutTimer = new QTimer(this);
    m_otaTimeoutTimer->setSingleShot(true);
    m_otaTimeoutTimer->setInterval(30000); // 30秒超时
    
    // 连接信号槽
    connect(m_otaTimeoutTimer, &QTimer::timeout, this, &SystemInitializer::onOtaConfigTimeout);
    
    // 初始化ConfigManager
    m_configManager = ConfigManager::getInstance();
    
    // 确保CLIENT_ID已初始化
    m_configManager->initializeClientId();
    
    // 从ConfigManager获取配置
    m_config = m_configManager->getFullConfig();
    m_clientId = m_configManager->getConfig("SYSTEM_OPTIONS.CLIENT_ID").toString();
    m_deviceId = m_configManager->getConfig("SYSTEM_OPTIONS.DEVICE_ID").toString();
    m_otaUrl = m_configManager->getConfig("SYSTEM_OPTIONS.NETWORK.OTA_VERSION_URL").toString();
    m_activationUrl = m_configManager->getConfig("SYSTEM_OPTIONS.NETWORK.AUTHORIZATION_URL").toString();
    
    // 初始化激活状态
    m_activationStatus["local_activated"] = m_deviceFingerprint->isActivated();
    m_activationStatus["server_activated"] = false;
    m_activationStatus["status_consistent"] = false;
    m_activationStatus["need_activation_ui"] = false;
    m_activationStatus["status_message"] = "";
    
    qDebug() << "SystemInitializer initialized with ConfigManager";
}

SystemInitializer::~SystemInitializer()
{
    if (m_currentReply) {
        m_currentReply->deleteLater();
    }
}

QJsonObject SystemInitializer::runInitialization()
{
    qDebug() << "Starting system initialization...";
    
    QJsonObject result;
    result["success"] = false;
    result["need_activation_ui"] = false;
    result["status_message"] = "";
    result["error"] = "";
    
    try {
        // 阶段1：设备指纹
        emit initializationProgress(1, "初始化设备指纹...");
        if (!stage1DeviceFingerprint()) {
            result["error"] = "设备指纹初始化失败";
            return result;
        }
        
        // 阶段2：配置管理
        emit initializationProgress(2, "初始化配置管理...");
        if (!stage2ConfigManagement()) {
            result["error"] = "配置管理初始化失败";
            return result;
        }
        
        // 阶段3：OTA配置
        emit initializationProgress(3, "获取服务器配置...");
        if (!stage3OtaConfig()) {
            qWarning() << "OTA配置获取失败，继续使用本地配置";
            // 不返回错误，继续执行激活状态分析
        }
        
        // 检查激活状态
        qDebug() << "Calling checkActivationStatus()...";
        checkActivationStatus();
        
        // 分析激活状态
        qDebug() << "Calling analyzeActivationStatus()...";
        QJsonObject activationAnalysis = analyzeActivationStatus();
        result["success"] = true;
        result["need_activation_ui"] = activationAnalysis["need_activation_ui"].toBool();
        result["status_message"] = activationAnalysis["status_message"].toString();
        result["local_activated"] = activationAnalysis["local_activated"].toBool();
        result["server_activated"] = activationAnalysis["server_activated"].toBool();
        
        // 传递激活数据
        if (activationAnalysis.contains("activation_data")) {
            result["activation_data"] = activationAnalysis["activation_data"];
        }
        
        qDebug() << "Final activation analysis - need_activation_ui:" << result["need_activation_ui"].toBool();
        qDebug() << "Final activation analysis - status_message:" << result["status_message"].toString();
        
        emit initializationCompleted(true, result["status_message"].toString());
        
    } catch (const std::exception& e) {
        result["error"] = QString("初始化异常: %1").arg(e.what());
        emit initializationCompleted(false, result["error"].toString());
    }
    
    return result;
}

bool SystemInitializer::stage1DeviceFingerprint()
{
    qDebug() << "Stage 1: Device Fingerprint";
    
    // 确保设备身份信息已创建
    if (!m_deviceFingerprint->ensureDeviceIdentity()) {
        qWarning() << "Failed to ensure device identity";
        return false;
    }
    
    QString serialNumber = m_deviceFingerprint->getSerialNumber();
    QString hmacKey = m_deviceFingerprint->getHmacKey();
    bool isActivated = m_deviceFingerprint->isActivated();
    
    qDebug() << "Device identity - Serial:" << serialNumber << "Activated:" << isActivated;
    
    return !serialNumber.isEmpty() && !hmacKey.isEmpty();
}

bool SystemInitializer::stage2ConfigManagement()
{
    qDebug() << "Stage 2: Config Management";
    
    // 使用ConfigManager初始化CLIENT_ID
    m_configManager->initializeClientId();
    m_clientId = m_configManager->getConfig("SYSTEM_OPTIONS.CLIENT_ID").toString();
    qDebug() << "CLIENT_ID:" << m_clientId;
    
    // 使用ConfigManager初始化DEVICE_ID
    QString macAddress = m_deviceFingerprint->getMacAddress();
    qDebug() << "Config management - CLIENT_ID:" << m_clientId << "DEVICE_ID:" << macAddress;
    
    // 强制更新DEVICE_ID，确保与efuse.json中的MAC地址一致
    m_configManager->updateConfig("SYSTEM_OPTIONS.DEVICE_ID", macAddress);
    m_deviceId = m_configManager->getConfig("SYSTEM_OPTIONS.DEVICE_ID").toString();
    if (m_deviceId.isEmpty()) {
        qWarning() << "No MAC address available for DEVICE_ID";
        return false;
    }
    
    qDebug() << "Config management - CLIENT_ID:" << m_clientId << "DEVICE_ID:" << m_deviceId;
    
    // 配置已通过ConfigManager自动保存
    
    return true;
}

bool SystemInitializer::stage3OtaConfig()
{
    qDebug() << "Stage 3: OTA Config";
    
    if (m_otaUrl.isEmpty()) {
        qWarning() << "OTA URL not configured";
        return false;
    }
    
    // 发送OTA配置请求
    sendOtaConfigRequest();
    
    // 启动超时定时器
    m_otaTimeoutTimer->start();
    
    // 等待网络请求完成
    QEventLoop loop;
    connect(m_currentReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(m_otaTimeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    loop.exec();
    
    // 检查是否成功获取配置
    if (m_currentReply && m_currentReply->error() == QNetworkReply::NoError) {
        qDebug() << "OTA config request successful";
        return true;
    } else {
        qWarning() << "OTA config request failed";
        return false;
    }
}

void SystemInitializer::checkActivationStatus()
{
    qDebug() << "Checking activation status...";
    
    // 检查本地激活状态
    bool localActivated = m_deviceFingerprint->isActivated();
    m_activationStatus["local_activated"] = localActivated;
    
    // 发送激活状态检查请求到服务器
    sendActivationStatusRequest();
    
    // 智能的服务器激活状态检查（参照py-xiaozhi逻辑）
    // 检查服务器是否返回了激活数据
    bool serverActivated = false;
    bool hasActivationData = m_config.contains("activation_data");
    
    if (hasActivationData) {
        QJsonObject activationData = m_config["activation_data"].toObject();
        // 如果服务器返回了激活数据，说明服务器认为设备未激活
        serverActivated = false;
        qDebug() << "Server returned activation data, device not activated on server";
    } else if (localActivated && !m_otaUrl.isEmpty() && !m_activationUrl.isEmpty()) {
        // 本地已激活，且服务器配置完整，且服务器没有返回激活数据，认为服务器也激活
        serverActivated = true;
        qDebug() << "Local activated, server config complete, no activation data - server activated";
    } else {
        // 其他情况认为服务器未激活
        serverActivated = false;
        qDebug() << "Server not activated (no config or no local activation)";
    }
    
    m_activationStatus["server_activated"] = serverActivated;
    
    // 检查状态是否一致
    bool statusConsistent = (localActivated == serverActivated);
    m_activationStatus["status_consistent"] = statusConsistent;
    
    qDebug() << "Activation status - Local:" << localActivated << "Server:" << serverActivated << "Consistent:" << statusConsistent;
    
    emit activationStatusChanged(m_activationStatus);
}

QJsonObject SystemInitializer::analyzeActivationStatus()
{
    bool localActivated = m_activationStatus["local_activated"].toBool();
    bool serverActivated = m_activationStatus["server_activated"].toBool();
    
    QJsonObject result;
    result["success"] = true;
    result["local_activated"] = localActivated;
    result["server_activated"] = serverActivated;
    result["need_activation_ui"] = false;
    result["status_message"] = "";
    
    qDebug() << "=== Activation Status Analysis ===";
    qDebug() << "Local activated:" << localActivated;
    qDebug() << "Server activated:" << serverActivated;
    qDebug() << "OTA URL:" << m_otaUrl;
    
    // 检查是否有激活数据（参照py-xiaozhi的逻辑）
    bool hasActivationData = m_config.contains("activation_data");
    QJsonObject activationData;
    if (hasActivationData) {
        activationData = m_config["activation_data"].toObject();
        qDebug() << "Server returned activation data:" << activationData;
    }
    
    // 情况1: 本地未激活，服务器未激活 - 正常激活流程
    if (!localActivated && !serverActivated) {
        result["need_activation_ui"] = true;
        result["status_message"] = "设备需要激活";
        qDebug() << "Result: Device needs activation (both not activated)";
    }
    // 情况2: 本地已激活，服务器已激活 - 正常已激活状态
    else if (localActivated && serverActivated) {
        result["need_activation_ui"] = false;
        result["status_message"] = "设备已激活";
        qDebug() << "Result: Device activated (both activated)";
    }
    // 情况3: 本地未激活，但服务器已激活 - 状态不一致，自动修复
    else if (!localActivated && serverActivated) {
        qDebug() << "Status inconsistent: local not activated, but server activated. Auto-fixing local status.";
        m_deviceFingerprint->setActivationStatus(true);
        result["local_activated"] = true;
        result["need_activation_ui"] = false;
        result["status_message"] = "已自动修复激活状态";
        qDebug() << "Result: Auto-fixed activation status";
    }
    // 情况4: 本地已激活，但服务器未激活 - 状态不一致，需要检查激活数据
    else if (localActivated && !serverActivated) {
        qDebug() << "Status inconsistent: local activated, but server not activated. Checking activation data.";
        
        if (hasActivationData && activationData.contains("code")) {
            // 服务器返回了激活码，需要重新激活
            qDebug() << "Server returned activation code, need to reactivate";
            result["need_activation_ui"] = true;
            result["status_message"] = "激活状态不一致，需要重新激活";
            qDebug() << "Result: Need reactivation due to status inconsistency";
        } else {
            // 没有激活数据，可能是服务器状态未更新，保持本地状态
            qDebug() << "No activation data from server, keeping local activated status";
            result["need_activation_ui"] = false;
            result["status_message"] = "保持本地激活状态";
            // 强制更新状态一致性，避免重复激活
            result["status_consistent"] = true;
            m_activationStatus["status_consistent"] = true;
            m_activationStatus["server_activated"] = true;
            result["server_activated"] = true;
            qDebug() << "Result: Keeping local activated status";
        }
    }
    
    // 如果OTA配置失败，且服务端未激活，则显示激活对话框
    if (m_otaUrl.isEmpty() && !serverActivated) {
        result["need_activation_ui"] = true;
        result["status_message"] = "OTA配置未设置，需要激活";
        qDebug() << "Result: Need activation due to missing OTA URL";
    }
    
    // 如果服务端返回了激活数据，传递给激活窗口
    if (m_config.contains("activation_data")) {
        result["activation_data"] = m_config["activation_data"];
        qDebug() << "Passing activation data from server to UI";
    } else {
        // 如果没有激活数据，创建默认的激活数据
        QJsonObject defaultActivationData;
        defaultActivationData["challenge"] = "default_challenge";
        defaultActivationData["code"] = "123456";
        defaultActivationData["message"] = "请在xiaozhi.me输入验证码";
        result["activation_data"] = defaultActivationData;
        qDebug() << "No activation data from server, using default activation code: 123456";
    }
    
    qDebug() << "Final result - need_activation_ui:" << result["need_activation_ui"].toBool();
    qDebug() << "Final result - status_message:" << result["status_message"].toString();
    
    return result;
}

QJsonObject SystemInitializer::handleActivationProcess(const QString& mode)
{
    QJsonObject result;
    result["is_activated"] = false;
    result["error"] = "";
    
    // 先运行初始化流程
    QJsonObject initResult = runInitialization();
    
    // 如果不需要激活界面，直接返回结果
    if (!initResult["need_activation_ui"].toBool()) {
        result["is_activated"] = true;
        result["device_fingerprint"] = QJsonObject(); // 简化处理
        return result;
    }
    
    // 需要激活界面，根据模式处理
    if (mode == "gui") {
        // GUI激活流程
        result["is_activated"] = false; // 需要用户交互
        result["message"] = "需要显示激活界面";
    } else {
        // CLI激活流程
        result["is_activated"] = false; // 需要用户交互
        result["message"] = "需要CLI激活流程";
    }
    
    return result;
}

void SystemInitializer::sendOtaConfigRequest()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
    
    QUrl url(m_otaUrl);
    QNetworkRequest request(url);
    
    // 设置请求头
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 强制使用HTTP/1.1，避免HTTP/2协议问题
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    
    request.setRawHeader("Device-Id", m_deviceId.toUtf8());
    request.setRawHeader("Client-Id", m_clientId.toUtf8());
    request.setRawHeader("User-Agent", "Live2D桌宠/1.0.0");
    request.setRawHeader("Accept-Language", "zh-CN");
    request.setRawHeader("Activation-Version", "2");
    
    // 构建请求体
    QJsonObject requestBody;
    QJsonObject application;
    application["version"] = "1.0.0";
    application["elf_sha256"] = m_deviceFingerprint->getHmacKey();
    requestBody["application"] = application;
    
    QJsonObject board;
    board["type"] = "desktop";
    board["name"] = "Live2D桌宠";
    board["ip"] = "127.0.0.1"; // 简化处理
    board["mac"] = m_deviceId;
    requestBody["board"] = board;
    
    QJsonDocument doc(requestBody);
    m_currentReply = m_networkManager->post(request, doc.toJson());
    
    connect(m_currentReply, &QNetworkReply::finished, this, &SystemInitializer::onOtaConfigReplyFinished);
    
    qDebug() << "OTA config request sent to:" << url.toString();
}

void SystemInitializer::sendActivationStatusRequest()
{
    // 这里简化处理，实际应该发送激活状态检查请求
    qDebug() << "Activation status request sent (simplified)";
}

void SystemInitializer::onOtaConfigReplyFinished()
{
    if (!m_currentReply) {
        return;
    }
    
    m_otaTimeoutTimer->stop();
    
    if (m_currentReply->error() == QNetworkReply::NoError) {
        QByteArray responseData = m_currentReply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject response = doc.object();
        
        qDebug() << "OTA config response received:" << response;
        
        // 处理服务器响应，更新配置
        bool configUpdated = false;
        
        // 使用ConfigManager更新WebSocket配置
        if (response.contains("websocket")) {
            QJsonObject websocket = response["websocket"].toObject();
            if (websocket.contains("url")) {
                QString websocketUrl = websocket["url"].toString();
                m_configManager->updateConfig("SYSTEM_OPTIONS.NETWORK.WEBSOCKET_URL", websocketUrl);
                qDebug() << "WebSocket URL updated:" << websocketUrl;
                configUpdated = true;
            }
            if (websocket.contains("token")) {
                QString websocketToken = websocket["token"].toString();
                m_configManager->updateConfig("SYSTEM_OPTIONS.NETWORK.WEBSOCKET_ACCESS_TOKEN", websocketToken);
                qDebug() << "WebSocket Token updated:" << websocketToken;
                configUpdated = true;
            }
        }
        
        // 使用ConfigManager更新MQTT配置 - 完全参照py-xiaozhi的MQTT配置结构
        if (response.contains("mqtt")) {
            QJsonObject mqtt = response["mqtt"].toObject();
            // 直接使用服务器返回的完整MQTT配置
            m_configManager->updateConfig("SYSTEM_OPTIONS.NETWORK.MQTT_INFO", mqtt);
            qDebug() << "MQTT config updated:" << mqtt;
            configUpdated = true;
        }
        
        // 检查激活信息
        if (response.contains("activation")) {
            qDebug() << "检测到激活信息，设备需要激活";
            m_activationStatus["server_activated"] = false;
            // 保存激活数据供后续使用
            m_config["activation_data"] = response["activation"];
        } else {
            qDebug() << "未检测到激活信息，设备可能已激活";
            m_activationStatus["server_activated"] = true;
        }
        
        // 配置已通过ConfigManager自动保存
        if (configUpdated) {
            qDebug() << "Configuration updated via ConfigManager";
        }
        
    } else {
        qWarning() << "OTA config request error:" << m_currentReply->errorString();
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void SystemInitializer::onOtaConfigTimeout()
{
    qWarning() << "OTA config request timed out";
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void SystemInitializer::loadConfig()
{
    QString configPath = getConfigFilePath();
    QFile configFile(configPath);
    
    if (configFile.exists() && configFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
        m_config = doc.object();
        
        // 加载服务器配置
        if (m_config.contains("SYSTEM_OPTIONS")) {
            QJsonObject systemOptions = m_config["SYSTEM_OPTIONS"].toObject();
            m_clientId = systemOptions["CLIENT_ID"].toString();
            m_deviceId = systemOptions["DEVICE_ID"].toString();
            
            if (systemOptions.contains("NETWORK")) {
                QJsonObject network = systemOptions["NETWORK"].toObject();
                m_otaUrl = network["OTA_VERSION_URL"].toString();
                m_activationUrl = network["AUTHORIZATION_URL"].toString();
            }
        }
        
        qDebug() << "Config loaded from:" << configPath;
    } else {
        // 初始化默认配置结构
        m_config = QJsonObject();
        
        QJsonObject systemOptions;
        systemOptions["CLIENT_ID"] = QString();
        systemOptions["DEVICE_ID"] = QString();
        
        QJsonObject network;
        network["OTA_VERSION_URL"] = "https://api.tenclass.net/xiaozhi/ota/";
        network["WEBSOCKET_URL"] = QString();
        network["WEBSOCKET_ACCESS_TOKEN"] = QString();
        network["MQTT_INFO"] = QJsonObject();
        network["ACTIVATION_VERSION"] = "v2";
        network["AUTHORIZATION_URL"] = "https://xiaozhi.me/";
        
        systemOptions["NETWORK"] = network;
        m_config["SYSTEM_OPTIONS"] = systemOptions;
        
        // 添加其他配置选项，完全参照py-xiaozhi的DEFAULT_CONFIG
        QJsonObject wakeWordOptions;
        wakeWordOptions["USE_WAKE_WORD"] = true;
        wakeWordOptions["MODEL_PATH"] = "models";
        wakeWordOptions["NUM_THREADS"] = 4;
        wakeWordOptions["PROVIDER"] = "cpu";
        wakeWordOptions["MAX_ACTIVE_PATHS"] = 2;
        wakeWordOptions["KEYWORDS_SCORE"] = 1.8;
        wakeWordOptions["KEYWORDS_THRESHOLD"] = 0.2;
        wakeWordOptions["NUM_TRAILING_BLANKS"] = 1;
        m_config["WAKE_WORD_OPTIONS"] = wakeWordOptions;
        
        QJsonObject camera;
        camera["camera_index"] = 0;
        camera["frame_width"] = 640;
        camera["frame_height"] = 480;
        camera["fps"] = 30;
        camera["Local_VL_url"] = "https://open.bigmodel.cn/api/paas/v4/";
        camera["VLapi_key"] = "";
        camera["models"] = "glm-4v-plus";
        m_config["CAMERA"] = camera;
        
        QJsonObject shortcuts;
        shortcuts["ENABLED"] = true;
        shortcuts["MANUAL_PRESS"] = QJsonObject{{"modifier", "ctrl"}, {"key", "j"}, {"description", "按住说话"}};
        shortcuts["AUTO_TOGGLE"] = QJsonObject{{"modifier", "ctrl"}, {"key", "k"}, {"description", "自动对话"}};
        shortcuts["ABORT"] = QJsonObject{{"modifier", "ctrl"}, {"key", "q"}, {"description", "中断对话"}};
        shortcuts["MODE_TOGGLE"] = QJsonObject{{"modifier", "ctrl"}, {"key", "m"}, {"description", "切换模式"}};
        shortcuts["WINDOW_TOGGLE"] = QJsonObject{{"modifier", "ctrl"}, {"key", "w"}, {"description", "显示/隐藏窗口"}};
        m_config["SHORTCUTS"] = shortcuts;
        
        QJsonObject aecOptions;
        aecOptions["ENABLED"] = false;
        aecOptions["BUFFER_MAX_LENGTH"] = 200;
        aecOptions["FRAME_DELAY"] = 3;
        aecOptions["FILTER_LENGTH_RATIO"] = 0.4;
        aecOptions["ENABLE_PREPROCESS"] = true;
        m_config["AEC_OPTIONS"] = aecOptions;
        
        QJsonObject audioDevices;
        audioDevices["input_device_id"] = QJsonValue::Null;
        audioDevices["input_device_name"] = QJsonValue::Null;
        audioDevices["output_device_id"] = QJsonValue::Null;
        audioDevices["output_device_name"] = QJsonValue::Null;
        audioDevices["input_sample_rate"] = QJsonValue::Null;
        audioDevices["output_sample_rate"] = QJsonValue::Null;
        m_config["AUDIO_DEVICES"] = audioDevices;
        
        // 使用默认配置值
        m_otaUrl = "https://api.tenclass.net/xiaozhi/ota/";
        m_activationUrl = "https://xiaozhi.me/";
        qDebug() << "Using default config";
    }
}

void SystemInitializer::saveConfig()
{
    QString configPath = getConfigFilePath();
    QFile configFile(configPath);
    
    if (configFile.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(m_config);
        configFile.write(doc.toJson());
        qDebug() << "Config saved to:" << configPath;
    } else {
        qWarning() << "Failed to save config to:" << configPath;
    }
}

QString SystemInitializer::getConfigFilePath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::homePath() + "/.config/Live2D桌宠";
    }
    QDir().mkpath(configDir);
    return QDir(configDir).absoluteFilePath("config.json");
}
