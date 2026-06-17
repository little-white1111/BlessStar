# BlessStar 快速上手指南

> 从创建业务项目到打包交付的完整流程。

---

## 目录

1. [创建新业务项目](#1-创建新业务项目)
2. [编辑业务配置](#2-编辑业务配置)
3. [配置验证](#3-配置验证)
4. [构建与打包](#4-构建与打包)
5. [交付与部署](#5-交付与部署)
6. [进阶](#6-进阶)

---

## 1. 创建新业务项目

使用项目脚手架模板创建新的业务项目：

```bash
# 复制模板到目标业务目录
cp -r templates/biz-project/ my-biz-project/

# 或者使用打包脚本中集成的创建功能
cd my-biz-project
```

> ![截图占位-1: 模板目录结构]()

新项目目录结构：

```
my-biz-project/
├── .bizrc                   # 编辑此文件设置项目信息
├── config/                  # 业务配置文件
├── schemas/                 # JSON Schema 定义
├── scripts/                 # 部署与验证脚本
└── output/                  # 生成产物
```

## 2. 编辑业务配置

### 2.1 基本信息

编辑 `.bizrc` 设置项目名称、版本号等元信息。

### 2.2 数据库配置

编辑 `config/database.json`：

```json
{
  "driver": "mysql",
  "host": "your-db-host",
  "port": 3306,
  "database": "your_db_name",
  "username": "your_username",
  "password": "your_password"
}
```

### 2.3 业务配置 Schema

编辑 `config/biz_config.schema.json` 定义业务配置结构，该文件是 BlessStar 配置中间件理解的业务数据格式。

### 2.4 门禁链配置

编辑 `config/gate_chain.json` 配置业务级门禁链，控制配置下发时的校验流程。

## 3. 配置验证

运行验证脚本检查配置完整性：

```bash
# Windows PowerShell
.\scripts\validate.ps1

# Linux/macOS
# bash scripts/validate.sh   # 如有对应 sh 版本
```

> ![截图占位-2: 验证通过输出]()

验证脚本将检查：
- 所有必需的配置文件是否存在
- `.bizrc` 格式是否完整
- 配置目录结构是否完整

## 4. 构建与打包

> 前提：已安装 CMake、Node.js、npm、electron-builder。

### 4.1 全量打包（推荐）

使用一键打包脚本：

```bash
# Windows
.\tools\package\build.ps1

# Linux/macOS
# bash tools/package/build.sh
```

脚本自动执行：
1. CMake Release 编译
2. 收集 C++ 产物（libmysql.dll、SSL DLL 等）到 `native/bin/`
3. 运行 electron-builder 生成安装包
4. 复制安装包到 `dist/` 目录

> ![截图占位-3: 打包成功输出]()

### 4.2 分步打包（调试用）

```bash
# 仅编译
cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . --config Release

# 仅 electron-builder 打包
cd app/editor && npx electron-builder --config electron-builder.yml
```

### 4.3 产物位置

安装包位于项目根目录的 `dist/` 文件夹：

| 平台 | 安装包类型 | 后缀 |
|------|-----------|------|
| Windows | NSIS 安装包 | `.exe` |
| macOS | DMG 映像 | `.dmg` |
| Linux | AppImage | `.AppImage` |

## 5. 交付与部署

1. 从 `dist/` 获取安装包
2. 在目标机器上安装 BlessStar Config Editor
3. 使用 Editor 打开 `my-biz-project/config/` 目录
4. 编辑业务配置后保存即可触发 BlessStar 配置管理流程

## 6. 进阶

### 6.1 自定义 Schema

在 `schemas/` 目录下添加自定义 JSON Schema 文件，BlessStar 将使用这些 Schema 验证业务配置的合法性。

### 6.2 部署脚本定制

编辑 `scripts/deploy.ps1` 添加实际的部署逻辑（上传、重启、健康检查等）。

### 6.3 Go 进一步了解

- [项目文档总览](./README.md)
- [架构方案选择记录](../架构方案选择记录.md)
- [契约定义与门禁体系](./contracts/index.json)

---

> **提示**: 本文档中的截图占位将在后续版本中替换为实际截图。
