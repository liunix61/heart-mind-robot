#include "LAppWavFileHandler.hpp"
#include <QObject>

// 简单的存根实现
LAppWavFileHandler::LAppWavFileHandler()
{
}

LAppWavFileHandler::~LAppWavFileHandler()
{
}

void LAppWavFileHandler::Start(const Live2D::Cubism::Framework::csmString& path)
{
    Q_UNUSED(path)
}

void LAppWavFileHandler::Start(std::shared_ptr<QByteArray>& data)
{
    Q_UNUSED(data)
}
