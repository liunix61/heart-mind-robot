//
// WebSocket版本的聊天对话框实现
// 使用WebSocket替代HTTP请求进行对话
//

#include "WebSocketChatDialog.h"
#include "DeskPetIntegration.h"
#include <QMouseEvent>
#include <QDebug>
#include <QDateTime>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QKeyEvent>

WebSocketChatDialog::WebSocketChatDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("WebSocket聊天");
    
    // 初始化时根据配置决定是否显示边框
    // 默认是无边框的，除非用户打开了"移动"模式
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setWindowFlag(Qt::NoDropShadowWindowHint);
    
    auto &model = resource_loader::get_instance();
    qDebug() << "Loading dialog config - Width:" << model.dialog_width << "Height:" << model.dialog_height;
    this->resize(model.dialog_width, model.dialog_height);
    this->move(model.dialog_x, model.dialog_y);
    qDebug() << "Dialog resized to:" << this->width() << "x" << this->height();
    
    m_deskPetIntegration = nullptr;
    m_connected = false;
    m_isRecording = false;
    m_audioInputManager = std::make_unique<AudioInputManager>();
    m_lastBotMessageTime = 0;
    m_lastUserMessageTime = 0;
    m_globalHotkey = nullptr;
    m_mousePressed = false;
    
    // 创建 QVBoxLayout 用于放置 QTextEdit 控件
    auto *layout = new QVBoxLayout(this);

    // 创建 QTextEdit 控件
    textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);
    
    // 微信风格背景
    textEdit->setStyleSheet(
            "QTextEdit {"
            "background-color: #F5F5F5; "  // 微信浅灰色背景
            "border: none; "
            "padding: 10px; "
            "font-family: 'PingFang SC', 'Helvetica Neue', 'STHeiti', sans-serif; "
            "font-size: 15px; "
            "}"
            "QScrollBar:vertical {"
            "    width: 6px; "
            "    background: transparent;"
            "}"
            "QScrollBar::handle:vertical {"
            "    background: #C0C0C0;"
            "    border-radius: 3px;"
            "    min-height: 20px;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "    height: 0px;"
            "}"
            "QScrollBar:horizontal {"
            "    height: 0px;"
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
    
    // 语音输入按钮（长按录音）
    voiceButton = new QPushButton("🎤", this);
    voiceButton->setFixedSize(34, 34);
    voiceButton->setToolTip("长按录音，松开发送");  // 添加提示
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
    
    // 使用长按模式：按下开始录音，松开停止录音
    connect(voiceButton, &QPushButton::pressed, this, &WebSocketChatDialog::startVoiceRecording);
    connect(voiceButton, &QPushButton::released, this, &WebSocketChatDialog::stopVoiceRecording);
    
    // 设置音频输入
    setupAudioInput();
    
    // 设置全局热键 (Cmd+Shift+V)
    m_globalHotkey = new GlobalHotkey(this);
    if (m_globalHotkey->registerHotkey()) {
        connect(m_globalHotkey, &GlobalHotkey::hotkeyPressed, 
                this, &WebSocketChatDialog::startVoiceRecording);
        connect(m_globalHotkey, &GlobalHotkey::hotkeyReleased, 
                this, &WebSocketChatDialog::stopVoiceRecording);
        qDebug() << "Global hotkey (Cmd+Shift+V) registered for voice input";
    } else {
        qWarning() << "Failed to register global hotkey";
    }
    
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
    connect(m_deskPetIntegration, &DeskPetIntegration::sttReceived,
            this, &WebSocketChatDialog::onSTTReceived);
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
        
        // 使用微信风格的用户消息气泡
        appendUserMessage(message);
        inputLine->clear();
        
        if (m_deskPetIntegration) {
            // 无论连接状态如何，都尝试发送消息
            // DeskPetIntegration会处理重连逻辑
            m_deskPetIntegration->sendTextMessage(message);
            qDebug() << "WebSocket: Sending message:" << message;
        } else {
            appendSystemMessage("DeskPetIntegration未初始化");
            sendButton->setEnabled(true);
        }
    }
}

void WebSocketChatDialog::BotReply(const QString &content) {
    // 使用微信风格的Bot消息气泡
    appendBotMessage(content);
    sendButton->setEnabled(true);
}

void WebSocketChatDialog::onWebSocketConnected() {
    m_connected = true;
    updateConnectionStatus();
    appendSystemMessage("连接成功，可以开始对话了！");
}

void WebSocketChatDialog::onWebSocketDisconnected() {
    m_connected = false;
    updateConnectionStatus();
    appendSystemMessage("连接已断开");
}

void WebSocketChatDialog::onWebSocketError(const QString &error) {
    m_connected = false;
    updateConnectionStatus();
    appendSystemMessage("连接错误: " + error);
}

void WebSocketChatDialog::onSTTReceived(const QString &text) {
    qDebug() << "STT received:" << text;
    
    if (!text.isEmpty()) {
        // 显示用户说的话（微信风格气泡）
        appendUserMessage(text);
    }
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
    // 这个方法不再使用，改用长按模式
    // 保留函数以防兼容性问题
}

void WebSocketChatDialog::startVoiceRecording() {
    if (!m_audioInputManager || !m_deskPetIntegration) {
        return;
    }
    
    if (m_isRecording) {
        return; // 已经在录音中
    }
    
    // 打断当前的TTS播放（如果正在说话）
    m_deskPetIntegration->interruptSpeaking();
    qDebug() << "Interrupted current speaking if any";
    
    // 发送开始监听消息到服务器（必须先发送才能接收音频）
    m_deskPetIntegration->startListening();
    qDebug() << "Sent startListening to server";
    
    // 开始录音
    if (!m_audioInputManager->startRecording()) {
        qWarning() << "Failed to start recording";
        // 如果录音失败，也要停止监听
        m_deskPetIntegration->stopListening();
        return;
    }
    
    qDebug() << "Voice input started (press and hold)";
}

void WebSocketChatDialog::stopVoiceRecording() {
    if (!m_audioInputManager || !m_deskPetIntegration) {
        return;
    }
    
    if (!m_isRecording) {
        return; // 没有在录音
    }
    
    // 停止录音
    m_audioInputManager->stopRecording();
    
    // 发送停止监听消息到服务器
    m_deskPetIntegration->stopListening();
    
    qDebug() << "Voice input stopped (released)";
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
    // 不再显示录音状态文本
}

void WebSocketChatDialog::onAudioError(const QString& error) {
    appendSystemMessage("音频错误: " + error);
    qWarning() << "Audio error:" << error;
}

void WebSocketChatDialog::updateVoiceButtonState() {
    // 长按模式下，按钮状态由CSS的:pressed自动处理
    // 不需要额外的状态切换
    // 按钮始终保持麦克风图标
}

// 添加用户消息（微信风格：右对齐绿色气泡）
void WebSocketChatDialog::appendUserMessage(const QString &message) {
    // 去重：如果和上一条用户消息相同且在1秒内，则忽略
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (message == m_lastUserMessage && (currentTime - m_lastUserMessageTime) < 1000) {
        qDebug() << "Duplicate user message filtered:" << message;
        return;
    }
    
    // 记录本次消息
    m_lastUserMessage = message;
    m_lastUserMessageTime = currentTime;
    
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    // 设置段落右对齐，添加上下间距
    QTextBlockFormat blockFormat;
    blockFormat.setAlignment(Qt::AlignRight);
    blockFormat.setTopMargin(8);      // 上边距
    blockFormat.setBottomMargin(8);   // 下边距
    cursor.insertBlock(blockFormat);
    
    // 插入气泡样式的文字
    QTextCharFormat charFormat;
    charFormat.setBackground(QColor("#95EC69"));
    charFormat.setForeground(QColor("#000000"));
    
    cursor.insertText(" " + message + " ", charFormat);
    
    scrollToBottom();
}

// 添加Bot消息（微信风格：左对齐白色气泡）
void WebSocketChatDialog::appendBotMessage(const QString &message) {
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    // 设置段落左对齐，添加上下间距
    QTextBlockFormat blockFormat;
    blockFormat.setAlignment(Qt::AlignLeft);
    blockFormat.setTopMargin(8);      // 上边距
    blockFormat.setBottomMargin(8);   // 下边距
    cursor.insertBlock(blockFormat);
    
    // 插入气泡样式的文字
    QTextCharFormat charFormat;
    charFormat.setBackground(QColor("#FFFFFF"));
    charFormat.setForeground(QColor("#000000"));
    
    cursor.insertText(" " + message + " ", charFormat);
    
    scrollToBottom();
}

// 添加系统消息（居中显示，小字体灰色）
void WebSocketChatDialog::appendSystemMessage(const QString &message) {
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    // 设置段落居中，添加更大的上下间距
    QTextBlockFormat blockFormat;
    blockFormat.setAlignment(Qt::AlignCenter);
    blockFormat.setTopMargin(12);     // 上边距（系统消息间距稍大）
    blockFormat.setBottomMargin(12);  // 下边距
    cursor.insertBlock(blockFormat);
    
    // 插入系统消息样式
    QTextCharFormat charFormat;
    charFormat.setForeground(QColor("#999999"));
    charFormat.setBackground(QColor("#F0F0F0"));
    charFormat.setFontPointSize(10);
    
    cursor.insertText(" " + message + " ", charFormat);
    
    scrollToBottom();
}

// 滚动到底部
void WebSocketChatDialog::scrollToBottom() {
    QTextCursor cursor = textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    textEdit->setTextCursor(cursor);
    textEdit->ensureCursorVisible();
}

// 不再需要键盘事件处理，改用全局热键
void WebSocketChatDialog::keyPressEvent(QKeyEvent *event) {
    QDialog::keyPressEvent(event);
}

void WebSocketChatDialog::keyReleaseEvent(QKeyEvent *event) {
    QDialog::keyReleaseEvent(event);
}

// 鼠标按下事件 - 开始拖动（只在无边框模式下有效）
void WebSocketChatDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // 检查点击的位置是否在输入控件或按钮上
        QWidget *widget = this->childAt(event->pos());
        if (widget && (widget == inputLine || 
                       widget == sendButton || 
                       widget == voiceButton || 
                       widget == textEdit)) {
            // 点击在控件上，不启动拖动
            QDialog::mousePressEvent(event);
            return;
        }
        
        // 只有在无边框模式下才允许拖动
        if (this->windowFlags() & Qt::FramelessWindowHint) {
            m_mousePressed = true;
            m_mousePos = event->globalPosition().toPoint() - this->pos();
            this->setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
    }
    QDialog::mousePressEvent(event);
}

// 鼠标移动事件 - 拖动窗口（只在无边框模式下有效）
void WebSocketChatDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_mousePressed && (event->buttons() & Qt::LeftButton) && 
        (this->windowFlags() & Qt::FramelessWindowHint)) {
        this->move(event->globalPosition().toPoint() - m_mousePos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

// 鼠标释放事件 - 停止拖动并保存位置
void WebSocketChatDialog::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_mousePressed) {
        m_mousePressed = false;
        this->setCursor(Qt::ArrowCursor);
        // 保存新位置
        resource_loader::get_instance().update_dialog_position(this->pos().x(), this->pos().y());
        qDebug() << "Dialog moved to:" << this->pos().x() << "," << this->pos().y();
        event->accept();
    }
    QDialog::mouseReleaseEvent(event);
}
