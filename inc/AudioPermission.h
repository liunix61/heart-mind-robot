#ifndef AUDIOPERMISSION_H
#define AUDIOPERMISSION_H

// 跨平台音频权限管理接口
class AudioPermission {
public:
    // 请求麦克风权限
    static bool requestMicrophonePermission();
    
    // 检查麦克风权限状态
    static bool checkMicrophonePermission();
};

#endif // AUDIOPERMISSION_H

