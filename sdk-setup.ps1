$env:JAVA_HOME = 'C:\android\jdk\jdk-17.0.14+7'
$env:ANDROID_HOME = 'C:\android\sdk'
$env:ANDROID_SDK_ROOT = 'C:\android\sdk'
$env:Path = 'C:\android\jdk\jdk-17.0.14+7\bin;C:\android\sdk\cmdline-tools\latest\bin;' + $env:Path

# Accept all licenses
$ynInput = ('y' * 50) -split '' | Where-Object { $_ } | ForEach-Object { "$_`n" }
$ynInput -join '' | & 'C:\android\sdk\cmdline-tools\latest\bin\sdkmanager.bat' --licenses | Out-Null

Write-Output "Licenses accepted"

# Install required SDK packages
& 'C:\android\sdk\cmdline-tools\latest\bin\sdkmanager.bat' "platform-tools" "platforms;android-28" "build-tools;34.0.0" "cmake;3.22.1"

Write-Output "SDK packages installed"
