//
// WebSocket版本的聊天对话框
// 使用WebSocket替代HTTP请求进行对话
//

#ifndef WEBSOCKETCHATDIALOG_H
#define WEBSOCKETCHATDIALOG_H

#include <QCoreApplication>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QObject>
#include <QScrollArea>
#include <QScrollBar>
#include <memory>
#include "ResourceLoader.hpp"
#include "AudioInputManager.hpp"
#include "GlobalHotkey.hpp"

class DeskPetIntegration;

class WebSocketChatDialog : public QDialog {
    Q_OBJECT

public:
    explicit WebSocketChatDialog(QWidget *parent = nullptr);
    ~WebSocketChatDialog() override;

    void BotReply(const QString &content);
    void setDeskPetIntegration(DeskPetIntegration *integration);

private slots:
    void sendMessage();
    void toggleVoiceInput();  // 保留但不使用
    void startVoiceRecording();  // 长按开始
    void stopVoiceRecording();   // 松开停止
    void onAudioDataEncoded(const QByteArray& encodedData);
    void onRecordingStateChanged(bool isRecording);
    void onAudioError(const QString& error);
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(const QString &error);
    void onBotReplyTextMessage(const QString &text);
    void onSTTReceived(const QString &text);  // 语音识别结果
    void onBotReplyAudioData(const QByteArray &audioData);
    void onPetEmotionChanged(const QString &emotion);
    void onPetMotionChanged(const QString &motion);
    void onPetExpressionChanged(const QString &expression);

private:
    QLineEdit *inputLine;
    QPushButton *sendButton;
    QPushButton *voiceButton;
    QTextEdit *textEdit;
    DeskPetIntegration *m_deskPetIntegration;
    bool m_connected;
    
    // 音频输入管理器
    std::unique_ptr<AudioInputManager> m_audioInputManager;
    bool m_isRecording;
    
    // 全局热键
    GlobalHotkey *m_globalHotkey;
    
    // 消息去重
    QString m_lastBotMessage;
    qint64 m_lastBotMessageTime;  // 毫秒时间戳
    QString m_lastUserMessage;     // 记录最后一条用户消息
    qint64 m_lastUserMessageTime;  // 用户消息时间戳
    
    // 鼠标拖动
    bool m_mousePressed;
    QPoint m_mousePos;
    
    void updateConnectionStatus();
    void setupConnections();
    void setupAudioInput();
    void updateVoiceButtonState();
    
    // 微信风格消息气泡
    void appendUserMessage(const QString &message);
    void appendBotMessage(const QString &message);
    void appendSystemMessage(const QString &message);  // 系统消息（居中显示）
    void scrollToBottom();
    
protected:
    // 重写键盘事件处理快捷键
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    
    // 重写鼠标事件实现拖动
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
};

#endif // WEBSOCKETCHATDIALOG_H
