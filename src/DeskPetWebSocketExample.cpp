// DeskPetWebSocketExample.cpp
// 展示如何在现有的Live2D桌宠项目中集成WebSocket客户端

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QStatusBar>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QThread>

#include "DeskPetIntegration.h"
#include "mainwindow.h"

class DeskPetWebSocketExample : public QMainWindow
{
    Q_OBJECT

public:
    explicit DeskPetWebSocketExample(QWidget *parent = nullptr);
    ~DeskPetWebSocketExample();

private slots:
    // 连接控制
    void onConnectClicked();
    void onDisconnectClicked();
    
    // 消息发送
    void onSendTextClicked();
    void onSendVoiceClicked();
    
    // 桌宠控制
    void onStartListeningClicked();
    void onStopListeningClicked();
    void onAbortSpeakingClicked();
    
    // 配置
    void onServerUrlChanged();
    void onAccessTokenChanged();
    void onAudioEnabledToggled(bool enabled);
    void onMicrophoneEnabledToggled(bool enabled);
    void onSpeakerEnabledToggled(bool enabled);
    void onAnimationEnabledToggled(bool enabled);
    
    // 桌宠集成信号
    void onDeskPetConnected();
    void onDeskPetDisconnected();
    void onDeskPetError(const QString &error);
    void onDeskPetBehaviorChanged(PetBehavior behavior);
    void onDeskPetDeviceStateChanged(DeviceState state);
    void onDeskPetMessageReceived(const QString &message);
    void onDeskPetAudioReceived(const QByteArray &audioData);
    void onDeskPetEmotionChanged(const QString &emotion);
    void onDeskPetPetInteraction(const QString &interaction);
    void onDeskPetAnimationRequested(const QString &animationName);
    void onDeskPetDebugMessage(const QString &message);
    
    // 定时器
    void onStatusUpdateTimeout();

private:
    void setupUI();
    void setupConnections();
    void updateStatus();
    void logMessage(const QString &message);
    void updateConnectionStatus();
    void updateBehaviorStatus();
    void updateDeviceStatus();
    
    // UI组件
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;
    
    // 连接控制
    QGroupBox *m_connectionGroup;
    QLineEdit *m_serverUrlEdit;
    QLineEdit *m_accessTokenEdit;
    QPushButton *m_connectButton;
    QPushButton *m_disconnectButton;
    QLabel *m_connectionStatusLabel;
    
    // 消息发送
    QGroupBox *m_messageGroup;
    QLineEdit *m_textInput;
    QPushButton *m_sendTextButton;
    QPushButton *m_sendVoiceButton;
    QTextEdit *m_messageLog;
    
    // 桌宠控制
    QGroupBox *m_controlGroup;
    QPushButton *m_startListeningButton;
    QPushButton *m_stopListeningButton;
    QPushButton *m_abortSpeakingButton;
    
    // 配置
    QGroupBox *m_configGroup;
    QCheckBox *m_audioEnabledCheck;
    QCheckBox *m_microphoneEnabledCheck;
    QCheckBox *m_speakerEnabledCheck;
    QCheckBox *m_animationEnabledCheck;
    
    // 状态显示
    QGroupBox *m_statusGroup;
    QLabel *m_behaviorStatusLabel;
    QLabel *m_deviceStatusLabel;
    QLabel *m_connectionStatusLabel2;
    
    // 核心组件
    DeskPetIntegration *m_deskPetIntegration;
    QTimer *m_statusUpdateTimer;
    
    // 状态变量
    bool m_connected;
    PetBehavior m_currentBehavior;
    DeviceState m_currentDeviceState;
};

DeskPetWebSocketExample::DeskPetWebSocketExample(QWidget *parent)
    : QMainWindow(parent)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_connectionGroup(nullptr)
    , m_serverUrlEdit(nullptr)
    , m_accessTokenEdit(nullptr)
    , m_connectButton(nullptr)
    , m_disconnectButton(nullptr)
    , m_connectionStatusLabel(nullptr)
    , m_messageGroup(nullptr)
    , m_textInput(nullptr)
    , m_sendTextButton(nullptr)
    , m_sendVoiceButton(nullptr)
    , m_messageLog(nullptr)
    , m_controlGroup(nullptr)
    , m_startListeningButton(nullptr)
    , m_stopListeningButton(nullptr)
    , m_abortSpeakingButton(nullptr)
    , m_configGroup(nullptr)
    , m_audioEnabledCheck(nullptr)
    , m_microphoneEnabledCheck(nullptr)
    , m_speakerEnabledCheck(nullptr)
    , m_animationEnabledCheck(nullptr)
    , m_statusGroup(nullptr)
    , m_behaviorStatusLabel(nullptr)
    , m_deviceStatusLabel(nullptr)
    , m_connectionStatusLabel2(nullptr)
    , m_deskPetIntegration(nullptr)
    , m_statusUpdateTimer(nullptr)
    , m_connected(false)
    , m_currentBehavior(PetBehavior::IDLE)
    , m_currentDeviceState(DeviceState::DISCONNECTED)
{
    setupUI();
    setupConnections();
    
    // 创建桌宠集成
    m_deskPetIntegration = new DeskPetIntegration(this);
    if (!m_deskPetIntegration->initialize(nullptr)) {
        QMessageBox::critical(this, "错误", "无法初始化桌宠集成");
        return;
    }
    
    // 设置定时器
    m_statusUpdateTimer = new QTimer(this);
    m_statusUpdateTimer->setInterval(1000); // 1秒
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &DeskPetWebSocketExample::onStatusUpdateTimeout);
    m_statusUpdateTimer->start();
    
    // 设置默认值
    m_serverUrlEdit->setText("wss://api.tenclass.net/xiaozhi/v1/");
    m_accessTokenEdit->setText("");
    
    // 更新状态
    updateStatus();
}

DeskPetWebSocketExample::~DeskPetWebSocketExample()
{
    if (m_deskPetIntegration) {
        m_deskPetIntegration->shutdown();
    }
}

void DeskPetWebSocketExample::setupUI()
{
    setWindowTitle("桌宠WebSocket客户端示例");
    setMinimumSize(800, 600);
    
    // 创建中央部件
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    // 连接控制组
    m_connectionGroup = new QGroupBox("连接控制", this);
    QVBoxLayout *connectionLayout = new QVBoxLayout(m_connectionGroup);
    
    QHBoxLayout *serverLayout = new QHBoxLayout();
    serverLayout->addWidget(new QLabel("服务器URL:"));
    m_serverUrlEdit = new QLineEdit(this);
    m_serverUrlEdit->setPlaceholderText("wss://api.tenclass.net/xiaozhi/v1/");
    serverLayout->addWidget(m_serverUrlEdit);
    connectionLayout->addLayout(serverLayout);
    
    QHBoxLayout *tokenLayout = new QHBoxLayout();
    tokenLayout->addWidget(new QLabel("访问令牌:"));
    m_accessTokenEdit = new QLineEdit(this);
    m_accessTokenEdit->setEchoMode(QLineEdit::Password);
    m_accessTokenEdit->setPlaceholderText("输入访问令牌");
    tokenLayout->addWidget(m_accessTokenEdit);
    connectionLayout->addLayout(tokenLayout);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_connectButton = new QPushButton("连接", this);
    m_disconnectButton = new QPushButton("断开", this);
    m_disconnectButton->setEnabled(false);
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    connectionLayout->addLayout(buttonLayout);
    
    m_connectionStatusLabel = new QLabel("未连接", this);
    m_connectionStatusLabel->setStyleSheet("color: red;");
    connectionLayout->addWidget(m_connectionStatusLabel);
    
    m_mainLayout->addWidget(m_connectionGroup);
    
    // 消息发送组
    m_messageGroup = new QGroupBox("消息发送", this);
    QVBoxLayout *messageLayout = new QVBoxLayout(m_messageGroup);
    
    QHBoxLayout *textLayout = new QHBoxLayout();
    m_textInput = new QLineEdit(this);
    m_textInput->setPlaceholderText("输入文本消息");
    textLayout->addWidget(m_textInput);
    m_sendTextButton = new QPushButton("发送文本", this);
    textLayout->addWidget(m_sendTextButton);
    messageLayout->addLayout(textLayout);
    
    QHBoxLayout *voiceLayout = new QHBoxLayout();
    m_sendVoiceButton = new QPushButton("发送语音", this);
    voiceLayout->addWidget(m_sendVoiceButton);
    voiceLayout->addStretch();
    messageLayout->addLayout(voiceLayout);
    
    m_messageLog = new QTextEdit(this);
    m_messageLog->setMaximumHeight(150);
    m_messageLog->setReadOnly(true);
    messageLayout->addWidget(m_messageLog);
    
    m_mainLayout->addWidget(m_messageGroup);
    
    // 桌宠控制组
    m_controlGroup = new QGroupBox("桌宠控制", this);
    QHBoxLayout *controlLayout = new QHBoxLayout(m_controlGroup);
    
    m_startListeningButton = new QPushButton("开始监听", this);
    m_stopListeningButton = new QPushButton("停止监听", this);
    m_abortSpeakingButton = new QPushButton("中止说话", this);
    
    controlLayout->addWidget(m_startListeningButton);
    controlLayout->addWidget(m_stopListeningButton);
    controlLayout->addWidget(m_abortSpeakingButton);
    
    m_mainLayout->addWidget(m_controlGroup);
    
    // 配置组
    m_configGroup = new QGroupBox("配置", this);
    QVBoxLayout *configLayout = new QVBoxLayout(m_configGroup);
    
    m_audioEnabledCheck = new QCheckBox("启用音频", this);
    m_audioEnabledCheck->setChecked(true);
    configLayout->addWidget(m_audioEnabledCheck);
    
    m_microphoneEnabledCheck = new QCheckBox("启用麦克风", this);
    m_microphoneEnabledCheck->setChecked(true);
    configLayout->addWidget(m_microphoneEnabledCheck);
    
    m_speakerEnabledCheck = new QCheckBox("启用扬声器", this);
    m_speakerEnabledCheck->setChecked(true);
    configLayout->addWidget(m_speakerEnabledCheck);
    
    m_animationEnabledCheck = new QCheckBox("启用动画", this);
    m_animationEnabledCheck->setChecked(true);
    configLayout->addWidget(m_animationEnabledCheck);
    
    m_mainLayout->addWidget(m_configGroup);
    
    // 状态显示组
    m_statusGroup = new QGroupBox("状态显示", this);
    QVBoxLayout *statusLayout = new QVBoxLayout(m_statusGroup);
    
    m_behaviorStatusLabel = new QLabel("行为: 空闲", this);
    statusLayout->addWidget(m_behaviorStatusLabel);
    
    m_deviceStatusLabel = new QLabel("设备状态: 断开", this);
    statusLayout->addWidget(m_deviceStatusLabel);
    
    m_connectionStatusLabel2 = new QLabel("连接状态: 未连接", this);
    statusLayout->addWidget(m_connectionStatusLabel2);
    
    m_mainLayout->addWidget(m_statusGroup);
    
    // 状态栏
    statusBar()->showMessage("就绪");
}

void DeskPetWebSocketExample::setupConnections()
{
    // 连接控制
    connect(m_connectButton, &QPushButton::clicked, this, &DeskPetWebSocketExample::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &DeskPetWebSocketExample::onDisconnectClicked);
    
    // 消息发送
    connect(m_sendTextButton, &QPushButton::clicked, this, &DeskPetWebSocketExample::onSendTextClicked);
    connect(m_sendVoiceButton, &QPushButton::clicked, this, &DeskPetWebSocketExample::onSendVoiceClicked);
    connect(m_textInput, &QLineEdit::returnPressed, this, &DeskPetWebSocketExample::onSendTextClicked);
    
    // 桌宠控制
    connect(m_startListeningButton, &QPushButton::clicked, this, &DeskPetWebSocketExample::onStartListeningClicked);
    connect(m_stopListeningButton, &QPushButton::clicked, this, &DeskPetWebSocketExample::onStopListeningClicked);
    connect(m_abortSpeakingButton, &QPushButton::clicked, this, &DeskPetWebSocketExample::onAbortSpeakingClicked);
    
    // 配置
    connect(m_serverUrlEdit, &QLineEdit::textChanged, this, &DeskPetWebSocketExample::onServerUrlChanged);
    connect(m_accessTokenEdit, &QLineEdit::textChanged, this, &DeskPetWebSocketExample::onAccessTokenChanged);
    connect(m_audioEnabledCheck, &QCheckBox::toggled, this, &DeskPetWebSocketExample::onAudioEnabledToggled);
    connect(m_microphoneEnabledCheck, &QCheckBox::toggled, this, &DeskPetWebSocketExample::onMicrophoneEnabledToggled);
    connect(m_speakerEnabledCheck, &QCheckBox::toggled, this, &DeskPetWebSocketExample::onSpeakerEnabledToggled);
    connect(m_animationEnabledCheck, &QCheckBox::toggled, this, &DeskPetWebSocketExample::onAnimationEnabledToggled);
}

void DeskPetWebSocketExample::onConnectClicked()
{
    if (!m_deskPetIntegration) return;
    
    // 设置服务器URL和访问令牌
    m_deskPetIntegration->setServerUrl(m_serverUrlEdit->text());
    m_deskPetIntegration->setAccessToken(m_accessTokenEdit->text());
    
    // 连接服务器
    if (m_deskPetIntegration->connectToServer()) {
        logMessage("正在连接服务器...");
        m_connectButton->setEnabled(false);
        m_disconnectButton->setEnabled(true);
    } else {
        logMessage("连接失败");
    }
}

void DeskPetWebSocketExample::onDisconnectClicked()
{
    if (!m_deskPetIntegration) return;
    
    m_deskPetIntegration->disconnectFromServer();
    logMessage("已断开连接");
    m_connectButton->setEnabled(true);
    m_disconnectButton->setEnabled(false);
}

void DeskPetWebSocketExample::onSendTextClicked()
{
    if (!m_deskPetIntegration || !m_deskPetIntegration->isConnected()) {
        logMessage("未连接到服务器");
        return;
    }
    
    QString text = m_textInput->text().trimmed();
    if (text.isEmpty()) {
        logMessage("请输入文本消息");
        return;
    }
    
    m_deskPetIntegration->sendTextMessage(text);
    logMessage("发送文本: " + text);
    m_textInput->clear();
}

void DeskPetWebSocketExample::onSendVoiceClicked()
{
    if (!m_deskPetIntegration || !m_deskPetIntegration->isConnected()) {
        logMessage("未连接到服务器");
        return;
    }
    
    // 这里应该实现语音录制功能
    logMessage("语音录制功能待实现");
}

void DeskPetWebSocketExample::onStartListeningClicked()
{
    if (!m_deskPetIntegration || !m_deskPetIntegration->isConnected()) {
        logMessage("未连接到服务器");
        return;
    }
    
    m_deskPetIntegration->startListening();
    logMessage("开始监听");
}

void DeskPetWebSocketExample::onStopListeningClicked()
{
    if (!m_deskPetIntegration) return;
    
    m_deskPetIntegration->stopListening();
    logMessage("停止监听");
}

void DeskPetWebSocketExample::onAbortSpeakingClicked()
{
    if (!m_deskPetIntegration) return;
    
    m_deskPetIntegration->abortSpeaking();
    logMessage("中止说话");
}

void DeskPetWebSocketExample::onServerUrlChanged()
{
    if (m_deskPetIntegration) {
        m_deskPetIntegration->setServerUrl(m_serverUrlEdit->text());
    }
}

void DeskPetWebSocketExample::onAccessTokenChanged()
{
    if (m_deskPetIntegration) {
        m_deskPetIntegration->setAccessToken(m_accessTokenEdit->text());
    }
}

void DeskPetWebSocketExample::onAudioEnabledToggled(bool enabled)
{
    if (m_deskPetIntegration) {
        m_deskPetIntegration->setAudioEnabled(enabled);
    }
}

void DeskPetWebSocketExample::onMicrophoneEnabledToggled(bool enabled)
{
    if (m_deskPetIntegration) {
        m_deskPetIntegration->setMicrophoneEnabled(enabled);
    }
}

void DeskPetWebSocketExample::onSpeakerEnabledToggled(bool enabled)
{
    if (m_deskPetIntegration) {
        m_deskPetIntegration->setSpeakerEnabled(enabled);
    }
}

void DeskPetWebSocketExample::onAnimationEnabledToggled(bool enabled)
{
    if (m_deskPetIntegration) {
        m_deskPetIntegration->setAnimationEnabled(enabled);
    }
}

void DeskPetWebSocketExample::onDeskPetConnected()
{
    logMessage("已连接到服务器");
    m_connected = true;
    updateConnectionStatus();
}

void DeskPetWebSocketExample::onDeskPetDisconnected()
{
    logMessage("已断开连接");
    m_connected = false;
    updateConnectionStatus();
}

void DeskPetWebSocketExample::onDeskPetError(const QString &error)
{
    logMessage("连接错误: " + error);
    m_connected = false;
    updateConnectionStatus();
}

void DeskPetWebSocketExample::onDeskPetBehaviorChanged(PetBehavior behavior)
{
    m_currentBehavior = behavior;
    updateBehaviorStatus();
    
    QString behaviorText;
    switch (behavior) {
    case PetBehavior::IDLE:
        behaviorText = "空闲";
        break;
    case PetBehavior::LISTENING:
        behaviorText = "监听";
        break;
    case PetBehavior::SPEAKING:
        behaviorText = "说话";
        break;
    case PetBehavior::THINKING:
        behaviorText = "思考";
        break;
    case PetBehavior::EXCITED:
        behaviorText = "兴奋";
        break;
    case PetBehavior::SAD:
        behaviorText = "悲伤";
        break;
    case PetBehavior::ANGRY:
        behaviorText = "愤怒";
        break;
    case PetBehavior::SLEEPING:
        behaviorText = "睡眠";
        break;
    default:
        behaviorText = "未知";
        break;
    }
    
    logMessage("行为变化: " + behaviorText);
}

void DeskPetWebSocketExample::onDeskPetDeviceStateChanged(DeviceState state)
{
    m_currentDeviceState = state;
    updateDeviceStatus();
    
    QString stateText;
    switch (state) {
    case DeviceState::IDLE:
        stateText = "空闲";
        break;
    case DeviceState::LISTENING:
        stateText = "监听";
        break;
    case DeviceState::SPEAKING:
        stateText = "说话";
        break;
    case DeviceState::CONNECTING:
        stateText = "连接中";
        break;
    case DeviceState::DISCONNECTED:
        stateText = "断开";
        break;
    default:
        stateText = "未知";
        break;
    }
    
    logMessage("设备状态变化: " + stateText);
}

void DeskPetWebSocketExample::onDeskPetMessageReceived(const QString &message)
{
    logMessage("收到消息: " + message);
}

void DeskPetWebSocketExample::onDeskPetAudioReceived(const QByteArray &audioData)
{
    logMessage("收到音频数据，大小: " + QString::number(audioData.size()) + " 字节");
}

void DeskPetWebSocketExample::onDeskPetEmotionChanged(const QString &emotion)
{
    logMessage("情绪变化: " + emotion);
}

void DeskPetWebSocketExample::onDeskPetPetInteraction(const QString &interaction)
{
    logMessage("桌宠交互: " + interaction);
}

void DeskPetWebSocketExample::onDeskPetAnimationRequested(const QString &animationName)
{
    logMessage("请求动画: " + animationName);
}

void DeskPetWebSocketExample::onDeskPetDebugMessage(const QString &message)
{
    logMessage("调试: " + message);
}

void DeskPetWebSocketExample::onStatusUpdateTimeout()
{
    updateStatus();
}

void DeskPetWebSocketExample::updateStatus()
{
    updateConnectionStatus();
    updateBehaviorStatus();
    updateDeviceStatus();
}

void DeskPetWebSocketExample::logMessage(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_messageLog->append(QString("[%1] %2").arg(timestamp, message));
    
    // 自动滚动到底部
    QTextCursor cursor = m_messageLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_messageLog->setTextCursor(cursor);
}

void DeskPetWebSocketExample::updateConnectionStatus()
{
    if (m_connected) {
        m_connectionStatusLabel->setText("已连接");
        m_connectionStatusLabel->setStyleSheet("color: green;");
        m_connectionStatusLabel2->setText("连接状态: 已连接");
    } else {
        m_connectionStatusLabel->setText("未连接");
        m_connectionStatusLabel->setStyleSheet("color: red;");
        m_connectionStatusLabel2->setText("连接状态: 未连接");
    }
}

void DeskPetWebSocketExample::updateBehaviorStatus()
{
    QString behaviorText;
    switch (m_currentBehavior) {
    case PetBehavior::IDLE:
        behaviorText = "空闲";
        break;
    case PetBehavior::LISTENING:
        behaviorText = "监听";
        break;
    case PetBehavior::SPEAKING:
        behaviorText = "说话";
        break;
    case PetBehavior::THINKING:
        behaviorText = "思考";
        break;
    case PetBehavior::EXCITED:
        behaviorText = "兴奋";
        break;
    case PetBehavior::SAD:
        behaviorText = "悲伤";
        break;
    case PetBehavior::ANGRY:
        behaviorText = "愤怒";
        break;
    case PetBehavior::SLEEPING:
        behaviorText = "睡眠";
        break;
    default:
        behaviorText = "未知";
        break;
    }
    
    m_behaviorStatusLabel->setText("行为: " + behaviorText);
}

void DeskPetWebSocketExample::updateDeviceStatus()
{
    QString stateText;
    switch (m_currentDeviceState) {
    case DeviceState::IDLE:
        stateText = "空闲";
        break;
    case DeviceState::LISTENING:
        stateText = "监听";
        break;
    case DeviceState::SPEAKING:
        stateText = "说话";
        break;
    case DeviceState::CONNECTING:
        stateText = "连接中";
        break;
    case DeviceState::DISCONNECTED:
        stateText = "断开";
        break;
    default:
        stateText = "未知";
        break;
    }
    
    m_deviceStatusLabel->setText("设备状态: " + stateText);
}

#include "DeskPetWebSocketExample.moc"
