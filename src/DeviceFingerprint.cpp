#include "DeviceFingerprint.h"
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QHostInfo>
#include <QNetworkInterface>

DeviceFingerprint* DeviceFingerprint::s_instance = nullptr;

DeviceFingerprint::DeviceFingerprint(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
{
    initializeFilePaths();
    ensureEfuseFile();
    m_initialized = true;
}

DeviceFingerprint::~DeviceFingerprint()
{
}

DeviceFingerprint* DeviceFingerprint::getInstance()
{
    if (!s_instance) {
        s_instance = new DeviceFingerprint();
    }
    return s_instance;
}

void DeviceFingerprint::initializeFilePaths()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QDir::homePath() + "/.config/Live2D桌宠";
    }
    
    QDir().mkpath(configDir);
    m_efuseFilePath = QDir(configDir).absoluteFilePath("efuse.json");
    
    qDebug() << "DeviceFingerprint config directory:" << configDir;
}

void DeviceFingerprint::ensureEfuseFile()
{
    qDebug() << "Checking efuse file:" << m_efuseFilePath;
    
    if (!QFile::exists(m_efuseFilePath)) {
        qDebug() << "efuse.json file does not exist, creating new file";
        createNewEfuseFile();
    } else {
        qDebug() << "efuse.json file exists, validating integrity";
        validateAndFixEfuseFile();
    }
}

void DeviceFingerprint::createNewEfuseFile()
{
    // 生成设备指纹
    QJsonObject fingerprint = generateFingerprint();
    QString macAddress = getMacAddressFromSystem();
    
    qDebug() << "=== Network Interface Selection Debug ===";
    qDebug() << "Selected MAC address:" << macAddress;
    
    // 生成序列号和HMAC密钥
    QString serialNumber = generateSerialNumber();
    QString hmacKey = generateHardwareHash();
    
    qDebug() << "Generated serial number:" << serialNumber;
    qDebug() << "Generated HMAC key:" << hmacKey.left(8) + "...";
    
    // 创建完整的efuse数据，完全参照py-xiaozhi的结构和字段顺序
    m_efuseData = QJsonObject();
    m_efuseData["mac_address"] = macAddress;
    m_efuseData["serial_number"] = serialNumber;
    m_efuseData["hmac_key"] = hmacKey;
    m_efuseData["activation_status"] = false;
    m_efuseData["device_fingerprint"] = fingerprint;
    
    // 保存数据
    if (saveEfuseData()) {
        qDebug() << "Created efuse configuration file:" << m_efuseFilePath;
    } else {
        qWarning() << "Failed to create efuse configuration file";
    }
}

void DeviceFingerprint::validateAndFixEfuseFile()
{
    if (!loadEfuseData()) {
        qWarning() << "Failed to load efuse data, recreating file";
        createNewEfuseFile();
        return;
    }
    
    // 检查必要字段是否存在
    QStringList requiredFields = {
        "mac_address", "serial_number", "hmac_key", 
        "activation_status", "device_fingerprint"
    };
    
    QStringList missingFields;
    for (const QString& field : requiredFields) {
        if (!m_efuseData.contains(field)) {
            missingFields.append(field);
        }
    }
    
    if (!missingFields.isEmpty()) {
        qWarning() << "efuse configuration file missing fields:" << missingFields;
        fixMissingFields(m_efuseData, missingFields);
        saveEfuseData();
    } else {
        qDebug() << "efuse configuration file integrity check passed";
    }
}

void DeviceFingerprint::fixMissingFields(QJsonObject& efuseData, const QStringList& missingFields)
{
    for (const QString& field : missingFields) {
        if (field == "device_fingerprint") {
            efuseData[field] = generateFingerprint();
        } else if (field == "mac_address") {
            efuseData[field] = getMacAddressFromSystem();
        } else if (field == "serial_number") {
            efuseData[field] = generateSerialNumber();
        } else if (field == "hmac_key") {
            efuseData[field] = generateHardwareHash();
        } else if (field == "activation_status") {
            efuseData[field] = false;
        }
    }
    
    qDebug() << "Fixed missing fields in efuse configuration";
}

QString DeviceFingerprint::getSerialNumber()
{
    if (!m_initialized) {
        ensureDeviceIdentity();
    }
    return m_efuseData["serial_number"].toString();
}

QString DeviceFingerprint::getHmacKey()
{
    if (!m_initialized) {
        ensureDeviceIdentity();
    }
    return m_efuseData["hmac_key"].toString();
}

QString DeviceFingerprint::getMacAddress()
{
    if (!m_initialized) {
        ensureDeviceIdentity();
    }
    return m_efuseData["mac_address"].toString();
}

bool DeviceFingerprint::isActivated()
{
    if (!m_initialized) {
        ensureDeviceIdentity();
    }
    return m_efuseData["activation_status"].toBool();
}

bool DeviceFingerprint::setActivationStatus(bool status)
{
    qDebug() << "=== setActivationStatus called with status:" << status;
    
    if (!m_initialized) {
        qDebug() << "DeviceFingerprint not initialized, calling ensureDeviceIdentity()";
        ensureDeviceIdentity();
    }
    
    qDebug() << "Updating efuse data: activation_status =" << status;
    m_efuseData["activation_status"] = status;
    m_efuseData["activation_time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    qDebug() << "Calling saveEfuseData()...";
    bool success = saveEfuseData();
    
    if (success) {
        emit activationStatusChanged(status);
        qDebug() << "Activation status successfully changed to:" << status;
    } else {
        qWarning() << "Failed to save activation status!";
    }
    
    return success;
}

QJsonObject DeviceFingerprint::generateFingerprint()
{
    QJsonObject fingerprint;
    // 完全参照py-xiaozhi的device_fingerprint结构
    fingerprint["system"] = "Darwin"; // 固定使用Darwin，与py-xiaozhi一致
    fingerprint["hostname"] = getHostname();
    fingerprint["mac_address"] = getMacAddressFromSystem();
    fingerprint["machine_id"] = generateMachineId(); // 使用UUID格式的machine_id
    
    return fingerprint;
}

QString DeviceFingerprint::generateHardwareHash()
{
    QJsonObject fingerprint = generateFingerprint();
    
    // 提取最不可变的硬件标识符
    QStringList identifiers;
    
    // 主机名
    QString hostname = fingerprint["hostname"].toString();
    if (!hostname.isEmpty()) {
        identifiers.append(hostname);
    }
    
    // MAC地址
    QString macAddress = fingerprint["mac_address"].toString();
    if (!macAddress.isEmpty()) {
        identifiers.append(macAddress);
    }
    
    // 机器ID
    QString machineId = fingerprint["machine_id"].toString();
    if (!machineId.isEmpty()) {
        identifiers.append(machineId);
    }
    
    // 如果没有任何标识符，使用系统信息作为备用
    if (identifiers.isEmpty()) {
        identifiers.append(QSysInfo::productType());
        qWarning() << "No hardware identifiers found, using system info as fallback";
    }
    
    // 将所有标识符连接起来并计算哈希值
    QString fingerprintStr = identifiers.join("||");
    return QCryptographicHash::hash(fingerprintStr.toUtf8(), QCryptographicHash::Sha256).toHex();
}

QString DeviceFingerprint::generateSerialNumber()
{
    QJsonObject fingerprint = generateFingerprint();
    
    // 优先使用主网卡MAC地址生成序列号
    QString macAddress = fingerprint["mac_address"].toString();
    
    if (macAddress.isEmpty()) {
        // 如果没有MAC地址，使用机器ID或主机名
        QString machineId = fingerprint["machine_id"].toString();
        QString hostname = fingerprint["hostname"].toString();
        
        QString identifier;
        if (!machineId.isEmpty()) {
            identifier = machineId.left(12); // 取前12位
        } else if (!hostname.isEmpty()) {
            identifier = hostname.replace("-", "").replace("_", "").left(12);
        } else {
            identifier = "unknown";
        }
        
        QString shortHash = QCryptographicHash::hash(identifier.toUtf8(), QCryptographicHash::Md5).toHex().left(8).toUpper();
        return QString("SN-%1-%2").arg(shortHash).arg(identifier.toUpper());
    }
    
    // 确保MAC地址为小写且没有冒号
    QString macClean = macAddress.toLower().replace(":", "");
    QString shortHash = QCryptographicHash::hash(macClean.toUtf8(), QCryptographicHash::Md5).toHex().left(8).toUpper();
    return QString("SN-%1-%2").arg(shortHash).arg(macClean);
}

QString DeviceFingerprint::generateHmac()
{
    return generateHardwareHash();
}

QString DeviceFingerprint::generateHmac(const QString& challenge)
{
    QString hmacKey = getHmacKey();
    if (hmacKey.isEmpty()) {
        qWarning() << "HMAC key is empty, cannot generate signature";
        return QString();
    }
    
    // 使用HMAC-SHA256算法生成签名
    QByteArray key = QByteArray::fromHex(hmacKey.toUtf8());
    QByteArray data = challenge.toUtf8();
    
    QByteArray signature = QCryptographicHash::hash(key + data, QCryptographicHash::Sha256);
    return signature.toHex();
}

bool DeviceFingerprint::ensureDeviceIdentity()
{
    if (!loadEfuseData()) {
        qWarning() << "Failed to load efuse data";
        return false;
    }
    
    QString serialNumber = m_efuseData["serial_number"].toString();
    QString hmacKey = m_efuseData["hmac_key"].toString();
    bool isActivated = m_efuseData["activation_status"].toBool();
    
    qDebug() << "Device identity - Serial:" << serialNumber << "Activated:" << isActivated;
    
    return !serialNumber.isEmpty() && !hmacKey.isEmpty();
}

bool DeviceFingerprint::loadEfuseData()
{
    QFile file(m_efuseFilePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open efuse file:" << m_efuseFilePath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) {
        qWarning() << "Failed to parse efuse file as JSON";
        return false;
    }
    
    m_efuseData = doc.object();
    return true;
}

bool DeviceFingerprint::saveEfuseData()
{
    QFile file(m_efuseFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open efuse file for writing:" << m_efuseFilePath;
        return false;
    }
    
    QJsonDocument doc(m_efuseData);
    qint64 bytesWritten = file.write(doc.toJson());
    
    if (bytesWritten == -1) {
        qWarning() << "Failed to write efuse data";
        return false;
    }
    
    qDebug() << "Efuse data saved to:" << m_efuseFilePath;
    return true;
}

QString DeviceFingerprint::getEfuseFilePath() const
{
    return m_efuseFilePath;
}

QString DeviceFingerprint::normalizeMacAddress(const QString& macAddress)
{
    if (macAddress.isEmpty()) {
        return macAddress;
    }
    
    // 移除所有可能的分隔符，只保留十六进制字符
    QString cleanMac;
    for (const QChar& c : macAddress) {
        if (c.isLetterOrNumber()) {
            cleanMac.append(c);
        }
    }
    
    // 确保长度为12个字符（6个字节的十六进制表示）
    if (cleanMac.length() != 12) {
        qWarning() << "MAC address length incorrect:" << macAddress << "->" << cleanMac;
        return macAddress.toLower();
    }
    
    // 重新格式化为标准的冒号分隔格式
    QString formattedMac;
    for (int i = 0; i < 12; i += 2) {
        if (i > 0) formattedMac.append(":");
        formattedMac.append(cleanMac.mid(i, 2));
    }
    
    return formattedMac.toLower();
}

QString DeviceFingerprint::getHostname()
{
    return QHostInfo::localHostName();
}

QString DeviceFingerprint::generateMachineId()
{
    // 生成UUID格式的machine_id，完全参照py-xiaozhi
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
}

QString DeviceFingerprint::getMacAddressFromSystem()
{
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    qDebug() << "Available network interfaces:";
    for (const QNetworkInterface& interface : interfaces) {
        qDebug() << "Interface:" << interface.name() 
                 << "MAC:" << interface.hardwareAddress()
                 << "Flags:" << interface.flags();
    }
    
    // 优先选择非回环接口的MAC地址，完全参照py-xiaozhi的逻辑
    // 优先选择以太网接口(en0, en1等)，然后按名称排序
    QStringList preferredInterfaces = {"en0", "en1", "en2", "en3", "en4", "en5", "en6", "en7"};
    QStringList otherInterfaces;
    
    // 收集所有有效接口
    for (const QNetworkInterface& interface : interfaces) {
        if (interface.name().toLower().startsWith("lo")) continue;
        if (!(interface.flags() & QNetworkInterface::IsUp) || 
            !(interface.flags() & QNetworkInterface::IsRunning)) continue;
        
        QString macAddress = interface.hardwareAddress();
        if (macAddress.isEmpty() || macAddress == "00:00:00:00:00:00") continue;
        
        if (preferredInterfaces.contains(interface.name())) {
            // 优先接口，按优先级排序
            continue;
        } else {
            otherInterfaces.append(interface.name());
        }
    }
    
    // 首先尝试优先接口
    for (const QString& interfaceName : preferredInterfaces) {
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
        if (!interface.isValid()) continue;
        if (!(interface.flags() & QNetworkInterface::IsUp) || 
            !(interface.flags() & QNetworkInterface::IsRunning)) continue;
        
        QString macAddress = interface.hardwareAddress();
        if (!macAddress.isEmpty() && macAddress != "00:00:00:00:00:00") {
            qDebug() << "Selected preferred interface:" << interface.name() << "MAC:" << macAddress;
            return normalizeMacAddress(macAddress);
        }
    }
    
    // 如果优先接口都不可用，按名称排序选择其他接口
    otherInterfaces.sort();
    for (const QString& interfaceName : otherInterfaces) {
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(interfaceName);
        if (!interface.isValid()) continue;
        
        QString macAddress = interface.hardwareAddress();
        if (!macAddress.isEmpty() && macAddress != "00:00:00:00:00:00") {
            qDebug() << "Selected fallback interface:" << interface.name() << "MAC:" << macAddress;
            return normalizeMacAddress(macAddress);
        }
    }
    
    qWarning() << "No valid MAC address found";
    return QString();
}
