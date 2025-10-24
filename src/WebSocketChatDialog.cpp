//
// WebSocket版本的聊天对话框实现
// 使用WebSocket替代HTTP请求进行对话
//

#include "WebSocketChatDialog.h"
#include "DeskPetIntegration.h"
#include <QMouseEvent>
#include <QDebug>
#include <QDateTime>

WebSocketChatDialog::WebSocketChatDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("WebSocket聊天");
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setWindowFlag(Qt::NoDropShadowWindowHint);
    
    auto &model = resource_loader::get_instance();
    this->resize(model.dialog_width, model.dialog_height);
    this->move(model.dialog_x, model.dialog_y);
    
    m_deskPetIntegration = nullptr;
    m_connected = false;
    m_isRecording = false;
    m_audioInputManager = std::make_unique<AudioInputManager>();
    m_lastBotMessageTime = 0;
    
    // 创建 QVBoxLayout 用于放置 QTextEdit 控件
    auto *layout = new QVBoxLayout(this);

    // 创建 QTextEdit 控件
    textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);

    textEdit->setStyleSheet(
            "QTextEdit {"
            "background-color: rgba(220, 240, 255, 50%); "
            "border: 1px solid #94B8FF; "
            "border-radius: 10px; "
            "padding: 1px 1px; "
            "font-family: 'STHeiti', sans-serif; font-size: 14px; color: #333333;"
            "}"
            "QScrollBar:vertical {"
            "    width: 0px; /* 隐藏垂直滚动条滑块宽度 */"
            "}"
            "QScrollBar:horizontal {"
            "    height: 0px; /* 隐藏水平滚动条滑块高度 */"
            "}"
    );

    // 添加文本内容
    layout->addWidget(textEdit);

    auto *inputLayout = new QHBoxLayout;
    inputLine = new QLineEdit(this);
    // 美化输入文本框
    inputLine->setStyleSheet(
            "QLineEdit {"
            "    border: 1px solid #94B8FF;"
            "    background: rgba(220, 240, 255, 30%);"
            "    border-radius: 5px;"
            "    padding: 5px;"
            "font-family: 'STHeiti', sans-serif; font-size: 14px; color: #333333;"
            "}"
    );

    inputLayout->addWidget(inputLine);
    
    sendButton = new QPushButton("Send", this);
    sendButton->setFixedHeight(34);
    // 美化按钮
    sendButton->setStyleSheet(
            "QPushButton {"
            "    background-color: #5082E8;"
            "    color: white;"
            "    border: 1px solid #5082E8;"
            "    border-radius: 5px;"
            "    padding: 5px 10px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #405F9E;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #304974;"
            "}"
    );
    inputLayout->addWidget(sendButton);
    
    // 语音输入按钮
    voiceButton = new QPushButton("🎤", this);
    voiceButton->setFixedSize(34, 34);
    voiceButton->setStyleSheet(
            "QPushButton {"
            "    background-color: #4CAF50;"
            "    color: white;"
            "    border: 1px solid #4CAF50;"
            "    border-radius: 5px;"
            "    padding: 0px;"
            "    font-size: 16px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #45A049;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #E53935;"
            "}"
    );
    inputLayout->addWidget(voiceButton);
    layout->addLayout(inputLayout);
    setLayout(layout);
    
    // 连接信号槽
    connect(sendButton, &QPushButton::clicked, this, &WebSocketChatDialog::sendMessage);
    connect(inputLine, &QLineEdit::returnPressed, this, &WebSocketChatDialog::sendMessage);
    connect(voiceButton, &QPushButton::clicked, this, &WebSocketChatDialog::toggleVoiceInput);
    
    // 设置音频输入
    setupAudioInput();
    
    // 设置初始状态
    updateConnectionStatus();
}

WebSocketChatDialog::~WebSocketChatDialog() {
    // 停止录音
    if (m_audioInputManager) {
        m_audioInputManager->stopRecording();
    }
    
    delete textEdit;
    delete inputLine;
    delete sendButton;
    delete voiceButton;
}

void WebSocketChatDialog::setDeskPetIntegration(DeskPetIntegration *integration) {
    m_deskPetIntegration = integration;
    if (m_deskPetIntegration) {
        setupConnections();
        updateConnectionStatus();
    }
}

void WebSocketChatDialog::setupConnections() {
    if (!m_deskPetIntegration) return;
    
    // 先断开所有可能存在的旧连接，避免重复连接
    disconnect(m_deskPetIntegration, nullptr, this, nullptr);
    
    // 连接WebSocket状态信号
    connect(m_deskPetIntegration, &DeskPetIntegration::connected, 
            this, &WebSocketChatDialog::onWebSocketConnected);
    connect(m_deskPetIntegration, &DeskPetIntegration::disconnected, 
            this, &WebSocketChatDialog::onWebSocketDisconnected);
    connect(m_deskPetIntegration, &DeskPetIntegration::connectionError, 
            this, &WebSocketChatDialog::onWebSocketError);
    
    // 连接消息接收信号
    connect(m_deskPetIntegration, &DeskPetIntegration::messageReceived, 
            this, &WebSocketChatDialog::onBotReplyTextMessage);
    connect(m_deskPetIntegration, &DeskPetIntegration::audioReceived, 
            this, &WebSocketChatDialog::onBotReplyAudioData);
    
    // 连接桌宠状态信号
    connect(m_deskPetIntegration, &DeskPetIntegration::emotionChanged, 
            this, &WebSocketChatDialog::onPetEmotionChanged);
    // 注意：petMotionChanged和petExpressionChanged信号在DeskPetIntegration中不存在
    // 如果需要这些信号，需要在DeskPetIntegration中添加
}

void WebSocketChatDialog::sendMessage() {
    QString message = inputLine->text();
    if (!message.isEmpty()) {
        sendButton->setEnabled(false);
        textEdit->append("You:\n " + message);
        inputLine->clear();
        
        // 滚动到底部
        QTextCursor cursor = textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        textEdit->setTextCursor(cursor);
        
        if (m_deskPetIntegration) {
            // 无论连接状态如何，都尝试发送消息
            // DeskPetIntegration会处理重连逻辑
            m_deskPetIntegration->sendTextMessage(message);
            qDebug() << "WebSocket: Sending message:" << message;
        } else {
            textEdit->append("Bot:\n DeskPetIntegration未初始化");
            sendButton->setEnabled(true);
        }
    }
}

void WebSocketChatDialog::BotReply(const QString &content) {
    textEdit->append("Bot:\n " + content);
    sendButton->setEnabled(true);
    
    // 滚动到底部
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    textEdit->setTextCursor(cursor);
}

void WebSocketChatDialog::onWebSocketConnected() {
    m_connected = true;
    updateConnectionStatus();
    textEdit->append("Bot:\n WebSocket连接已建立，可以开始对话了！");
}

void WebSocketChatDialog::onWebSocketDisconnected() {
    m_connected = false;
    updateConnectionStatus();
    textEdit->append("Bot:\n WebSocket连接已断开");
}

void WebSocketChatDialog::onWebSocketError(const QString &error) {
    m_connected = false;
    updateConnectionStatus();
    textEdit->append("Bot:\n WebSocket连接错误: " + error);
}

void WebSocketChatDialog::onBotReplyTextMessage(const QString &text) {
    qDebug() << "### WebSocketChatDialog::onBotReplyTextMessage called ###";
    qDebug() << "    this pointer:" << this;
    qDebug() << "    Text:" << text;
    
    // 过滤纯表情消息（只包含emoji的消息）
    // 因为表情已经通过Live2D模型表现，不需要在对话框中显示
    QString trimmedText = text.trimmed();
    
    // 检查是否是纯表情（只包含emoji字符）
    // 简化逻辑：如果消息长度<=3且包含高Unicode字符，认为是纯表情
    bool isOnlyEmoji = false;
    if (trimmedText.length() <= 3) {
        isOnlyEmoji = true;
        for (const QChar &c : trimmedText) {
            // 如果包含普通ASCII字符、中文等，则不是纯表情
            if (c.unicode() < 0x2000 || (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FFF)) {
                if (!c.isSpace()) {
                    isOnlyEmoji = false;
                    break;
                }
            }
        }
    }
    
    qDebug() << "    isOnlyEmoji:" << isOnlyEmoji;
    
    // 只显示非纯表情消息
    if (!isOnlyEmoji && !trimmedText.isEmpty()) {
        // 去重：如果和上一条消息相同且在2秒内，则忽略
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (trimmedText == m_lastBotMessage && (currentTime - m_lastBotMessageTime) < 2000) {
            qDebug() << "    >>> Duplicate message filtered:" << trimmedText;
            return;
        }
        
        // 记录本次消息
        m_lastBotMessage = trimmedText;
        m_lastBotMessageTime = currentTime;
        
        qDebug() << "    >>> Will display message:" << text;
        BotReply(text);
    } else {
        qDebug() << "    >>> Message filtered (emoji or empty)";
    }
}

void WebSocketChatDialog::onBotReplyAudioData(const QByteArray &audioData) {
    // 音频已经在DeskPetIntegration中播放了，这里不需要再播放，避免重复
    // 只记录日志即可
    qDebug() << "WebSocketChatDialog: Audio received, size:" << audioData.size() << "bytes (already playing)";
    // 不要再次调用 playAudioData，否则会重复播放
}

void WebSocketChatDialog::onPetEmotionChanged(const QString &emotion) {
    // 情绪变化已经通过Live2D表情展示，不需要在对话框中显示
    Q_UNUSED(emotion);
    // textEdit->append("Bot:\n [桌宠情绪变化: " + emotion + "]");
}

void WebSocketChatDialog::onPetMotionChanged(const QString &motion) {
    Q_UNUSED(motion)
    // 这个方法暂时不使用，因为DeskPetIntegration中没有对应的信号
}

void WebSocketChatDialog::onPetExpressionChanged(const QString &expression) {
    Q_UNUSED(expression)
    // 这个方法暂时不使用，因为DeskPetIntegration中没有对应的信号
}

void WebSocketChatDialog::updateConnectionStatus() {
    if (m_connected) {
        sendButton->setText("Send (已连接)");
        sendButton->setStyleSheet(
                "QPushButton {"
                "    background-color: #4CAF50;"
                "    color: white;"
                "    border: 1px solid #4CAF50;"
                "    border-radius: 5px;"
                "    padding: 5px 10px;"
                "}"
        );
        voiceButton->setEnabled(true);
    } else {
        sendButton->setText("Send (未连接)");
        sendButton->setStyleSheet(
                "QPushButton {"
                "    background-color: #f44336;"
                "    color: white;"
                "    border: 1px solid #f44336;"
                "    border-radius: 5px;"
                "    padding: 5px 10px;"
                "}"
        );
        voiceButton->setEnabled(false);
    }
}

void WebSocketChatDialog::setupAudioInput() {
    if (!m_audioInputManager) {
        qWarning() << "WebSocketChatDialog: audio input manager is null";
        return;
    }
    
    // 初始化音频输入管理器（16kHz, 单声道, 20ms帧）
    qDebug() << "WebSocketChatDialog: initializing audio input manager...";
    if (!m_audioInputManager->initialize(16000, 1, 20)) {
        qWarning() << "WebSocketChatDialog: Failed to initialize audio input manager";
        voiceButton->setEnabled(false);
        return;
    }
    
    // 连接信号
    qDebug() << "WebSocketChatDialog: connecting audio signals...";
    connect(m_audioInputManager.get(), &AudioInputManager::audioDataEncoded,
            this, &WebSocketChatDialog::onAudioDataEncoded);
    connect(m_audioInputManager.get(), &AudioInputManager::recordingStateChanged,
            this, &WebSocketChatDialog::onRecordingStateChanged);
    connect(m_audioInputManager.get(), &AudioInputManager::errorOccurred,
            this, &WebSocketChatDialog::onAudioError);
    
    // 配置WebRTC处理
    qDebug() << "WebSocketChatDialog: configuring WebRTC...";
    m_audioInputManager->configureWebRTC(false, true, true); // AEC关闭, NS开启, HighPass开启
    m_audioInputManager->setWebRTCEnabled(true);
    
    qDebug() << "WebSocketChatDialog: Audio input setup completed";
}

void WebSocketChatDialog::toggleVoiceInput() {
    if (!m_audioInputManager) {
        return;
    }
    
    if (m_isRecording) {
        // 停止录音
        m_audioInputManager->stopRecording();
        qDebug() << "Voice input stopped";
    } else {
        // 开始录音
        if (!m_audioInputManager->startRecording()) {
            textEdit->append("Bot:\n 无法开始录音，请检查麦克风权限");
            qWarning() << "Failed to start recording";
            return;
        }
        textEdit->append("You:\n [正在录音...]");
        qDebug() << "Voice input started";
    }
}

void WebSocketChatDialog::onAudioDataEncoded(const QByteArray& encodedData) {
    if (!m_connected || !m_deskPetIntegration) {
        qWarning() << "Cannot send audio: not connected";
        return;
    }
    
    // 发送音频数据到服务器
    if (m_deskPetIntegration && m_deskPetIntegration->isConnected()) {
        // 通过WebSocket发送二进制音频数据
        m_deskPetIntegration->sendAudioData(encodedData);
        qDebug() << "Sent audio data:" << encodedData.size() << "bytes";
    }
}

void WebSocketChatDialog::onRecordingStateChanged(bool isRecording) {
    m_isRecording = isRecording;
    updateVoiceButtonState();
    
    if (!isRecording) {
        textEdit->append("You:\n [录音结束]");
    }
}

void WebSocketChatDialog::onAudioError(const QString& error) {
    textEdit->append("Bot:\n 音频错误: " + error);
    qWarning() << "Audio error:" << error;
}

void WebSocketChatDialog::updateVoiceButtonState() {
    if (m_isRecording) {
        voiceButton->setText("⏹");
        voiceButton->setStyleSheet(
                "QPushButton {"
                "    background-color: #E53935;"
                "    color: white;"
                "    border: 1px solid #E53935;"
                "    border-radius: 5px;"
                "    padding: 5px;"
                "    font-size: 16px;"
                "}"
        );
    } else {
        voiceButton->setText("🎤");
        voiceButton->setStyleSheet(
                "QPushButton {"
                "    background-color: #4CAF50;"
                "    color: white;"
                "    border: 1px solid #4CAF50;"
                "    border-radius: 5px;"
                "    padding: 5px;"
                "    font-size: 16px;"
                "}"
                "QPushButton:hover {"
                "    background-color: #45A049;"
                "}"
        );
    }
}
