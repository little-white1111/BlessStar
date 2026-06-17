#!/usr/bin/env bash
# ============================================================
# BlessStar 一键打包脚本（Linux/macOS bash）
# 步骤 1: CMake 编译 Release 版本
# 步骤 2: 收集 C++ 产物到 native/bin/
# 步骤 3: 运行 electron-builder 打包
# 步骤 4: 将最终安装包复制到 dist/
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
NATIVE_DIR="$PROJECT_ROOT/app/editor/native"
NATIVE_BIN="$NATIVE_DIR/bin"
EDITOR_DIR="$PROJECT_ROOT/app/editor"
DIST_DIR="$PROJECT_ROOT/dist"

echo "[build.sh] === BlessStar 一键打包开始 ==="

# ---- 步骤 1: CMake 编译 Release ----
echo "[build.sh] 步骤 1/4: CMake 编译 Release ..."
mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" > /dev/null
cmake "$PROJECT_ROOT" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
popd > /dev/null
echo "[build.sh] ✅ 步骤 1 完成"

# ---- 步骤 2: 收集编译产物 ----
echo "[build.sh] 步骤 2/4: 收集 C++ 产物到 native/bin/ ..."
mkdir -p "$NATIVE_BIN"

DLL_PATTERNS=("libmysql.dll" "libssl-3-x64.dll" "libcrypto-3-x64.dll")
FOUND_ANY=0
for dll in "${DLL_PATTERNS[@]}"; do
    SRC=$(find "$BUILD_DIR" -name "$dll" -print -quit 2>/dev/null || true)
    if [ -n "$SRC" ]; then
        cp -f "$SRC" "$NATIVE_BIN/$dll"
        echo "  复制: $SRC -> $NATIVE_BIN/$dll"
        FOUND_ANY=1
    else
        echo "  [WARN] 未找到 $dll，跳过" >&2
    fi
done

if [ "$FOUND_ANY" -eq 0 ]; then
    echo "  [提示] 编译产物中未找到 DLL，跳过收集（可手动放置到 native/bin/）"
fi
echo "[build.sh] ✅ 步骤 2 完成"

# ---- 步骤 3: 运行 electron-builder 打包 ----
echo "[build.sh] 步骤 3/4: electron-builder 打包 ..."
pushd "$EDITOR_DIR" > /dev/null
npx electron-builder --config electron-builder.yml
popd > /dev/null
echo "[build.sh] ✅ 步骤 3 完成"

# ---- 步骤 4: 复制安装包到 dist/ ----
echo "[build.sh] 步骤 4/4: 复制安装包到 dist/ ..."
mkdir -p "$DIST_DIR"

RELEASE_DIR="$EDITOR_DIR/release"
if [ -d "$RELEASE_DIR" ]; then
    find "$RELEASE_DIR" \( -name "*.exe" -o -name "*.dmg" -o -name "*.AppImage" -o -name "*.msi" -o -name "*.zip" \) -exec cp -f {} "$DIST_DIR/" \;
    echo "  [OK] 安装包已复制到 $DIST_DIR"
else
    echo "  [WARN] 未找到安装包，请检查 electron-builder 输出目录" >&2
fi
echo "[build.sh] ✅ 步骤 4 完成"

echo "[build.sh] === BlessStar 一键打包完成 ==="
echo "安装包位置: $DIST_DIR"
