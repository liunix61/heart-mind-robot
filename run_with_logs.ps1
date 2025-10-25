# 运行程序并捕获日志到文件
Write-Host "启动程序并捕获日志..."

# 切换到程序目录
Set-Location ".\build\bin\Release"

# 运行程序并重定向输出
Start-Process -FilePath ".\HeartMindRobot.exe" -Wait -RedirectStandardOutput "program_output.log" -RedirectStandardError "program_error.log"

Write-Host "程序运行完成，日志已保存到:"
Write-Host "- 标准输出: program_output.log"
Write-Host "- 错误输出: program_error.log"

# 显示最新的日志内容
Write-Host "`n=== 最新错误日志 ==="
Get-Content "program_error.log" | Select-Object -Last 20

Write-Host "`n=== 最新输出日志 ==="
Get-Content "program_output.log" | Select-Object -Last 20
