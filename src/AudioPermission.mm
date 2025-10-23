#include "AudioPermission.h"
#include <QCoreApplication>
#include <QDebug>

#ifdef __APPLE__
#include <AVFoundation/AVFoundation.h>

bool AudioPermission::requestMicrophonePermission()
{
    // macOS 10.14+ 需要请求麦克风权限
    if (@available(macOS 10.14, *)) {
        __block bool permissionGranted = false;
        __block bool requestCompleted = false;
        
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
            permissionGranted = granted;
            requestCompleted = true;
            
            if (granted) {
                qDebug() << "Microphone permission granted";
            } else {
                qWarning() << "Microphone permission denied";
            }
        }];
        
        // 等待权限请求完成（简单的同步等待）
        // 注意：在生产环境中应该使用更好的异步处理方式
        while (!requestCompleted) {
            QCoreApplication::processEvents();
        }
        
        return permissionGranted;
    }
    
    // 旧版本macOS不需要权限
    return true;
}

bool AudioPermission::checkMicrophonePermission()
{
    if (@available(macOS 10.14, *)) {
        AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
        
        switch (status) {
            case AVAuthorizationStatusAuthorized:
                return true;
            case AVAuthorizationStatusDenied:
            case AVAuthorizationStatusRestricted:
                return false;
            case AVAuthorizationStatusNotDetermined:
                // 尚未请求权限
                return false;
            default:
                return false;
        }
    }
    
    // 旧版本macOS不需要权限
    return true;
}

#else
// 非macOS平台
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
#endif

