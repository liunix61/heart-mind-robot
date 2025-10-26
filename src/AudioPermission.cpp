#include "AudioPermission.hpp"
#include <QCoreApplication>
#include <QDebug>

// Windows和Linux平台实现
bool AudioPermission::requestMicrophonePermission()
{
    // Windows和Linux通常不需要显式请求权限
    return true;
}

bool AudioPermission::checkMicrophonePermission()
{
    // Windows和Linux通常不需要显式检查权限
    return true;
}
