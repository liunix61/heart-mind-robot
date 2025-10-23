//
// WebSocket版本的聊天对话框实现
// 使用WebSocket替代HTTP请求进行对话
//

#include "WebSocketChatDialog.h"
#include "DeskPetIntegration.h"
#include <QMouseEvent>
#include <QDebug>

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
    layout->addLayout(inputLayout);
    setLayout(layout);
    
    // 连接信号槽
    connect(sendButton, &QPushButton::clicked, this, &WebSocketChatDialog::sendMessage);
    connect(inputLine, &QLineEdit::returnPressed, this, &WebSocketChatDialog::sendMessage);
    
    // 设置初始状态
    updateConnectionStatus();
}

WebSocketChatDialog::~WebSocketChatDialog() {
    delete textEdit;
    delete inputLine;
    delete sendButton;
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
        
        if (m_deskPetIntegration && m_deskPetIntegration->isConnected()) {
            // 使用WebSocket发送消息
            m_deskPetIntegration->sendTextMessage(message);
            qDebug() << "WebSocket: Sending message:" << message;
        } else {
            textEdit->append("Bot:\n WebSocket连接未建立，无法发送消息");
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
    BotReply(text);
}

void WebSocketChatDialog::onBotReplyAudioData(const QByteArray &audioData) {
    // 音频已经在DeskPetIntegration中播放了，这里不需要再播放，避免重复
    // 只记录日志即可
    qDebug() << "WebSocketChatDialog: Audio received, size:" << audioData.size() << "bytes (already playing)";
    // 不要再次调用 playAudioData，否则会重复播放
}

void WebSocketChatDialog::onPetEmotionChanged(const QString &emotion) {
    textEdit->append("Bot:\n [桌宠情绪变化: " + emotion + "]");
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
    }
}

#include "WebSocketChatDialog.moc"
