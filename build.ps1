param([string]$Task = "assembleDebug")

$env:JAVA_HOME = 'C:\android\jdk\jdk-17.0.14+7'
$env:ANDROID_HOME = 'C:\android\sdk'
$env:ANDROID_SDK_ROOT = 'C:\android\sdk'
$env:ANDROID_NDK_HOME = 'C:\android\android-ndk-r27d'
$env:Path = "C:\android\jdk\jdk-17.0.14+7\bin;C:\android\gradle\gradle-8.7\bin;C:\android\sdk\platform-tools;" + $env:Path

Set-Location C:\fireplay
& gradle $Task --no-daemon 2>&1
