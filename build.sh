#!/bin/bash

# Initialize variables

BOLD='\033[1m'
GRN='\033[01;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[01;31m'
RST='\033[0m'
ORIGIN_DIR=$(pwd)
BUILD_PREF_COMPILER='clang'
BUILD_PREF_COMPILER_VERSION='proton'
TOOLCHAIN=$(pwd)/build-shit/toolchain
# export environment variables
export_env_vars() {
    export KBUILD_BUILD_USER=Kohei
    export KBUILD_BUILD_HOST=Izumi
    
    export ARCH=arm64
    export SUBARCH=arm64
    export ANDROID_MAJOR_VERSION=r
    export PLATFORM_VERSION=11.0.0
    export $ARCH
    
    # CCACHE
    export USE_CCACHE=1
    export PATH="/usr/lib/ccache/bin/:$PATH"
    export CCACHE_SLOPPINESS="file_macro,locale,time_macros"
    export CCACHE_NOHASHDIR="true"
    export CCACHE_DIR=/mnt/45A15FA43FC33C00/Kernel-Dev/ccache-2
    export CROSS_COMPILE=aarch64-linux-gnu-
    export CROSS_COMPILE_ARM32=arm-linux-gnueabi-
    export CC=${BUILD_PREF_COMPILER}
}

script_echo() {
    echo "  $1"
}
exit_script() {
    kill -INT $$
}
add_deps() {
    echo -e "${CYAN}"
    if [ ! -d $(pwd)/build-shit ]
    then
        script_echo "Create build-shit folder"
        mkdir $(pwd)/build-shit
    fi
    
    if [ ! -d $(pwd)/build-shit/toolchain ]
    then
        script_echo "Downloading proton-clang...."
        git clone https://github.com/TenSeventy7/exynos9610_toolchains_fresh.git ${TOOLCHAIN} --single-branch -b ${BUILD_PREF_COMPILER_VERSION} --depth 1 2>&1 | sed 's/^/     /'
    fi
    verify_toolchain_install
}
verify_toolchain_install() {
    script_echo " "
    if [[ -d "${TOOLCHAIN}" ]]; then
        script_echo "I: Toolchain found at default location"
        export PATH="${TOOLCHAIN}/bin:$PATH"
        export LD_LIBRARY_PATH="${TOOLCHAIN}/lib:$LD_LIBRARY_PATH"
    else
        script_echo "I: Toolchain not found"
        script_echo "   Downloading recommended toolchain at ${TOOLCHAIN}..."
        add_deps
    fi
}
build_kernel_image() {
    script_echo " "
    read -p "Write the Kernel version: " KV
    
    script_echo 'Building CosmicFresh Kernel For M21'
    make -C $(pwd) CC=${BUILD_PREF_COMPILER} AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip -j$((`nproc`+1)) M21_defconfig 2>&1 | sed 's/^/     /'
    make -C $(pwd) CC=${BUILD_PREF_COMPILER} AR=llvm-ar NM=llvm-nm OBJCOPY=llvm-objcopy OBJDUMP=llvm-objdump STRIP=llvm-strip -j$((`nproc`+1)) 2>&1 | sed 's/^/     /'
}
build_flashable_zip() {
    if [[ -e "$(pwd)/arch/arm64/boot/Image" ]]; then
        script_echo " "
        script_echo "I: Building kernel image..."
        echo -e "${GRN}"
        rm -f $(pwd)/CosmicFresh/{Image, *.zip}
        cp -r $(pwd)/arch/arm64/boot/Image CosmicFresh/Image
        cd $(pwd)/CosmicFresh/
        zip -r9 "CosmicFresh-R$KV.zip" anykernel.sh META-INF tools version Image
        cd ../..
        rm -f $(pwd)/arch/arm64/boot/Image        
    else
        echo -e "${RED}"
        script_echo "E: Image not built!"
        script_echo "   Errors can be fround from above."
        sleep 3
        exit_script
    fi
}
add_deps
export_env_vars
build_kernel_image
build_flashable_zip
