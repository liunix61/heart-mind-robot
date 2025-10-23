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
#include "resource_loader.hpp"

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
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(const QString &error);
    void onBotReplyTextMessage(const QString &text);
    void onBotReplyAudioData(const QByteArray &audioData);
    void onPetEmotionChanged(const QString &emotion);
    void onPetMotionChanged(const QString &motion);
    void onPetExpressionChanged(const QString &expression);

private:
    QLineEdit *inputLine;
    QPushButton *sendButton;
    QTextEdit *textEdit;
    DeskPetIntegration *m_deskPetIntegration;
    bool m_connected;
    
    void updateConnectionStatus();
    void setupConnections();
};

#endif // WEBSOCKETCHATDIALOG_H
