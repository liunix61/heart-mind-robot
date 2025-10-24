#include "OpusDecoder.h"
#include <QObject>

// 简单的存根实现
OpusDecoder::OpusDecoder(QObject *parent) : QObject(parent)
{
}

OpusDecoder::~OpusDecoder()
{
}

bool OpusDecoder::initialize(int sampleRate, int channels)
{
    Q_UNUSED(sampleRate)
    Q_UNUSED(channels)
    return true;
}

QByteArray OpusDecoder::decode(const QByteArray& encodedData)
{
    Q_UNUSED(encodedData)
    return QByteArray();
}

