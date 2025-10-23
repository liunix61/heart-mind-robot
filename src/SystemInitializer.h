#ifndef SYSTEMINITIALIZER_H
#define SYSTEMINITIALIZER_H

#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include "DeviceFingerprint.h"
#include "ConfigManager.h"

class SystemInitializer : public QObject
{
    Q_OBJECT

public:
    enum InitializationStage {
        DEVICE_FINGERPRINT = 0,
        CONFIG_MANAGEMENT = 1,
        OTA_CONFIG = 2,
        ACTIVATION = 3
    };
    Q_ENUM(InitializationStage)

    explicit SystemInitializer(QObject *parent = nullptr);
    ~SystemInitializer();

    // 运行完整的初始化流程
    QJsonObject runInitialization();
    
    // 获取激活状态信息
    QJsonObject getActivationStatus() const { return m_activationStatus; }
    
    // 分析激活状态，决定后续流程
    QJsonObject analyzeActivationStatus();
    
    // 处理激活流程
    QJsonObject handleActivationProcess(const QString& mode = "gui");
    
    // 获取组件实例
    DeviceFingerprint* getDeviceFingerprint() const { return m_deviceFingerprint; }

signals:
    void initializationProgress(int stage, const QString& message);
    void initializationCompleted(bool success, const QString& message);
    void activationStatusChanged(const QJsonObject& status);

private slots:
    void onOtaConfigReplyFinished();
    void onOtaConfigTimeout();

private:
    // 初始化阶段
    bool stage1DeviceFingerprint();
    bool stage2ConfigManagement();
    bool stage3OtaConfig();
    
    // 激活状态检查
    void checkActivationStatus();
    void updateActivationStatus();
    
    // 网络请求
    void sendOtaConfigRequest();
    void sendActivationStatusRequest();
    
    // 配置管理
    void loadConfig();
    void saveConfig();
    QString getConfigFilePath() const;

private:
    DeviceFingerprint* m_deviceFingerprint;
    ConfigManager* m_configManager;
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QTimer* m_otaTimeoutTimer;
    
    // 状态管理
    InitializationStage m_currentStage;
    QJsonObject m_activationStatus;
    QJsonObject m_config;
    
    // 服务器配置
    QString m_otaUrl;
    QString m_activationUrl;
    QString m_clientId;
    QString m_deviceId;
};

#endif // SYSTEMINITIALIZER_H
