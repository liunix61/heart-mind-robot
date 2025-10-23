#ifndef DEVICEFINGERPRINT_H
#define DEVICEFINGERPRINT_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QHash>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QHostInfo>
#include <QNetworkInterface>

class DeviceFingerprint : public QObject
{
    Q_OBJECT

public:
    static DeviceFingerprint* getInstance();
    ~DeviceFingerprint();

    // 设备身份管理
    QString getSerialNumber();
    QString getHmacKey();
    QString getMacAddress();
    bool isActivated();
    bool setActivationStatus(bool status);
    
    // 设备指纹生成
    QJsonObject generateFingerprint();
    QString generateHardwareHash();
    QString generateSerialNumber();
    QString generateHmac();
    QString generateHmac(const QString& challenge);
    
    // 确保设备身份
    bool ensureDeviceIdentity();
    
    // 文件管理
    bool loadEfuseData();
    bool saveEfuseData();
    QString getEfuseFilePath() const;

signals:
    void deviceIdentityChanged();
    void activationStatusChanged(bool activated);

private:
    explicit DeviceFingerprint(QObject *parent = nullptr);
    Q_DISABLE_COPY(DeviceFingerprint)
    
    void initializeFilePaths();
    void ensureEfuseFile();
    void createNewEfuseFile();
    void validateAndFixEfuseFile();
    void fixMissingFields(QJsonObject& efuseData, const QStringList& missingFields);
    
    QString normalizeMacAddress(const QString& macAddress);
    QString getHostname();
    QString getMacAddressFromSystem();
    QString generateMachineId();
    
    static DeviceFingerprint* s_instance;
    
    QJsonObject m_efuseData;
    QString m_efuseFilePath;
    bool m_initialized;
};

#endif // DEVICEFINGERPRINT_H
