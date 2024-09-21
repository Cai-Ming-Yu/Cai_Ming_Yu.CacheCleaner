#!/bin/sh

# build Cache Cleaner
cd jni
ndk-build -j10

# make build dir
cd ../
rm -rf build
cp -rf source build

# copy Cache Cleaner
mkdir -p build/bin
cp -rf libs/arm64-v8a build/bin/arm64
cp -rf libs/armeabi-v7a build/bin/arm
cp -rf libs/x86_64 build/bin/x86_64
cp -rf libs/x86 build/bin/x86
cp -rf libs/riscv64 build/bin/riscv64

# process file
cp -rf encrypt/cmd.sh encrypt.sh
sed -i 's/\$(date \x27+%Y-%m-%d \%H:\%M:\%S\x27)/0000-01-01 00:00:00/g' encrypt.sh
chmod 0777 busybox
./busybox dos2unix encrypt.sh verify.sh changeTime.sh
./busybox sh encrypt.sh overfiles 3 build
#./busybox sh encrypt.sh file 3 build/META-INF/com/google/android/update-binary build/META-INF/com/google/android/update-binary
./busybox echo -en "\nSKIPUNZIP=1" >>build/customize.sh
sh verify.sh
cp -rf ../README.md build/README.md
cp -rf changelog.md build/changelog.md
sh changeTime.sh

# pack file
cd build
zip -r -9 CacheCleaner-Mod.zip .
mv -f CacheCleaner-Mod.zip ../CacheCleaner-Mod.zip

# exit
exit 0
