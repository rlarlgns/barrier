#!/bin/bash
# 1. x86_64용 도커 이미지 빌드 (buildx 사용)
IMAGE_NAME="barrier-linux-builder-amd64"
echo "Building x86_64 Docker Image ($IMAGE_NAME) using buildx..."
docker buildx build --platform linux/amd64 -t $IMAGE_NAME --load . || exit 1

# 2. 컨테이너 내부에서 x86_64용 빌드 및 AppImage 생성
echo "Starting x86_64 Build and AppImage creation inside Container..."
docker run --rm --platform linux/amd64 -v "$(pwd)":/src $IMAGE_NAME bash -c "
    # 빌드 디렉토리 초기화
    rm -rf /src/build_linux_x86 && mkdir -p /src/build_linux_x86 && cd /src/build_linux_x86 && \
    cmake -DBARRIER_BUILD_GUI=ON -DCMAKE_BUILD_TYPE=Release .. && \
    make -j4 && \
    
    # AppDir 구조 생성
    rm -rf /src/AppDir_x86 && mkdir -p /src/AppDir_x86/usr/bin && \
    cp /src/build_linux_x86/bin/barrier /src/AppDir_x86/usr/bin/ && \
    cp /src/build_linux_x86/bin/barriers /src/AppDir_x86/usr/bin/ && \
    cp /src/build_linux_x86/bin/barrierc /src/AppDir_x86/usr/bin/ && \
    cp /src/res/barrier.desktop /src/AppDir_x86/ && \
    cp /src/res/barrier.png /src/AppDir_x86/ && \
    
    # AppImage 생성 (사전 추출된 linuxdeployqt 사용)
    export VERSION=2.4.0-serial-x86_64 && \
    /opt/linuxdeployqt/AppRun /src/AppDir_x86/barrier.desktop -appimage -executable=/src/AppDir_x86/usr/bin/barriers -executable=/src/AppDir_x86/usr/bin/barrierc
"

# 결과물 확인 및 권한 수정
generated_file=$(find build_linux_x86 -name "Barrier-*-x86_64.AppImage" | head -n 1)
if [ -n "$generated_file" ]; then
    final_name="Barrier-2.4.0-serial-x86_64.AppImage"
    mv "$generated_file" "./$final_name"
    chown $USER:staff "./$final_name" || true
    chmod 755 "./$final_name"
    xattr -cr "./$final_name"
    echo "========================================"
    echo "SUCCESS: x86_64 AppImage created!"
    echo "Location: $(pwd)/$final_name"
    echo "========================================"
else
    echo "ERROR: AppImage creation failed."
    exit 1
fi
