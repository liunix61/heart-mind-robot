#ifndef SIMPLEACTIVATIONWINDOW_H
#define SIMPLEACTIVATIONWINDOW_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "ConfigManager.h"
#include "DeviceFingerprint.h"

class SimpleActivationWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SimpleActivationWindow(const QJsonObject& activationData, QWidget *parent = nullptr);
    ~SimpleActivationWindow();

    // 启动激活流程
    void startActivation();
    
    // 检查激活状态
    bool isActivated() const { return m_isActivated; }

signals:
    void activationCompleted(bool success);
    void activationCancelled();

private slots:
    void onCancelClicked();
    void onCopyCodeClicked();
    void onJumpToActivationClicked();
    void onNetworkReplyFinished();
    void onActivationTimeout();

private:
    void setupUI();
    void updateUI();
    void generateDeviceInfo();
    void startActivationProcess();
    void checkActivationStatus();
    void saveActivationConfig();
    void loadActivationConfig();
    void showMessage(const QString& message);
    
    // 网络请求
    void sendActivationRequest();
    void sendStatusCheckRequest();

private:
    // UI组件
    QLabel* m_titleLabel;
    QLabel* m_statusLabel;
    QLineEdit* m_activationCodeEdit;
    QPushButton* m_copyButton;
    QPushButton* m_cancelButton;
    QTextEdit* m_logText;
    
    // 布局
    QVBoxLayout* m_mainLayout;
    
    // 状态管理
    bool m_isActivated;
    bool m_isActivating;
    QString m_deviceId;
    QString m_activationCode;
    QString m_serverUrl;
    
    // py-xiaozhi激活流程相关
    QString m_clientId;
    QString m_serialNumber;
    QString m_challenge;
    QString m_hmacSignature;
    QString m_verificationCode;
    
    // 网络管理
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    
    // 定时器
    QTimer* m_activationTimer;
    QTimer* m_statusTimer;
    
    // 配置
    QJsonObject m_config;
};

#endif // SIMPLEACTIVATIONWINDOW_H
