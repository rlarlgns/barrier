#!/bin/bash
set -e

# 1. 빌드 디렉토리 준비
mkdir -p /src/build_linux_x86
cd /src/build_linux_x86

# 2. CMake 및 빌드 수행
echo "Compiling Barrier for x86_64..."
cmake -DBARRIER_BUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc 2>/dev/null || echo 4)

# 3. AppDir 구조 생성
echo "Preparing AppDir..."
cd /src
rm -rf AppDir_x86
mkdir -p AppDir_x86/usr/bin
cp build_linux_x86/bin/barrier AppDir_x86/usr/bin/
cp build_linux_x86/bin/barriers AppDir_x86/usr/bin/
cp build_linux_x86/bin/barrierc AppDir_x86/usr/bin/
cp res/barrier.desktop AppDir_x86/
cp res/barrier.png AppDir_x86/

# 4. linuxdeployqt 다운로드 및 압축 해제 (에뮬레이션 문제 회피)
echo "Downloading and extracting linuxdeployqt..."
wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage" -O linuxdeployqt.AppImage
chmod +x linuxdeployqt.AppImage
./linuxdeployqt.AppImage --appimage-extract

# 5. AppImage 생성 실행
echo "Creating AppImage..."
export VERSION=2.4.0-serial-x86_64
./squashfs-root/AppRun /src/AppDir_x86/barrier.desktop -appimage -executable=/src/AppDir_x86/usr/bin/barriers -executable=/src/AppDir_x86/usr/bin/barrierc

# 6. 결과물 이동
echo "Cleaning up and moving final file..."
generated_file=$(find . -name "Barrier-*-x86_64.AppImage" | grep -v "linuxdeployqt" | head -n 1)
if [ -n "$generated_file" ]; then
    mv "$generated_file" /src/Barrier-Final-x86_64.AppImage
    echo "SUCCESS: Barrier-Final-x86_64.AppImage created."
else
    echo "ERROR: Final AppImage not found."
    exit 1
fi
