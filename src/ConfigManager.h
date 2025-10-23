#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QString>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QUuid>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager* getInstance();
    ~ConfigManager();

    // 配置访问方法
    QVariant getConfig(const QString& path, const QVariant& defaultValue = QVariant()) const;
    bool updateConfig(const QString& path, const QVariant& value);
    bool reloadConfig();
    
    // 初始化方法
    void initializeClientId();
    void initializeDeviceIdFromFingerprint(const QString& macAddress);
    
    // 配置管理
    QJsonObject getFullConfig() const { return m_config; }
    bool saveConfig();
    bool loadConfig();
    
    // 内部方法
    void updateNestedObject(QJsonObject& obj, const QStringList& parts, const QString& lastKey, const QVariant& value);

private:
    explicit ConfigManager(QObject *parent = nullptr);
    Q_DISABLE_COPY(ConfigManager)

    void initFilePaths();
    void ensureRequiredDirectories();
    QJsonObject loadConfigFromFile() const;
    QJsonObject mergeConfigs(const QJsonObject& defaultConfig, const QJsonObject& customConfig) const;
    QString generateUuid() const;

    static ConfigManager* s_instance;
    QDir m_configDir;
    QString m_configFilePath;
    QJsonObject m_config;
    bool m_initialized;
    
    // 默认配置结构，完全参照py-xiaozhi
    static const QJsonObject DEFAULT_CONFIG;
};

#endif // CONFIGMANAGER_H
