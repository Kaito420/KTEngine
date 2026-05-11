import os, subprocess
vswhere = r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
result = subprocess.run([vswhere, '-latest', '-requires', 'Microsoft.Component.MSBuild', '-find', r'MSBuild\**\Bin\MSBuild.exe'], capture_output=True, text=True)
msbuild = result.stdout.strip().split('\n')[0]
print('Found MSBuild at:', msbuild)
os.system(f'"{msbuild}" /p:Configuration=Debug /p:Platform=x64 KTEngine.sln')
