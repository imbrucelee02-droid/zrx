# 中望 CAD (ZWCAD 2025) AI 表格/标题栏识别与重构转换 C++ 插件项目交付文档
## 1. 项目概述
本项目为基于中望 CAD 2025 ZRX SDK 开发的 C++ 动态链接库插件（`simple_table_prase.zrx`）。该插件在 CAD 内部内置了一个轻量级、无依赖的 WinSock2 HTTP REST API 服务器（默认监听 `18088` 端口），实现了 Web 前端与 CAD 绘图环境的无缝交互、AI 智能识别、数据编辑确认、原生 `ZcDbTable` 表格绘制以及旧图纸实体的自动擦除。
### 核心设计目标
1. **轻量与解耦**: 采用前端网页（Streamlit / Vue / React）+ C++ 后端 HTTP API 架构，界面 UI 与 CAD 底层逻辑完全分离。
2. **多线程安全与无死锁**: 使用 ZRX SDK 应用程序上下文（Application Context）调度与文档锁（Document Lock），确保网络线程与 CAD UI 主线程的高效平滑交互。
3. **彻底的真实流程**: 拒绝任何伪数据与假模板，全流程接入真实 DWG 实体框选提取与 Dify 大模型 API 工作流。
4. **精细的数据隔离**: 严格隔离 BOM 表 8 个标准字段与标题栏 26 个标准字段。

---

## 2. 系统架构与交互流转全图

```
+-----------------------------------------------------------------------------------+
|                                  Web 前端 UI                                      |
+-----------------------------------------------------------------------------------+
       | (Step 2: POST /api/select_and_process)   ^ (返回 26字段/8字段 + bbox + 句柄)
       v                                          |
+-----------------------------------------------------------------------------------+
|               ZRX C++ 后端 HTTP REST API 服务 (127.0.0.1:18088)                  |
|                           [ZrxHttpServer.cpp]                                     |
+-----------------------------------------------------------------------------------+
       | (主线程调度 executeInApplicationContext)   ^ (返回物理选区与识别数据)
       v                                          |
+-----------------------------------------------------------------------------------+
|               CAD 视口真实交互与实体提取 [CadSelectProcessor.cpp]                  |
|  - 视口单次画框选择 (acedGetCorner)                                                |
|  - 交叉选择集收集实体 Handle 句柄 (acedSSGet "C")                                  |
|  - 调用真实 Dify 大模型 API 工作流 (http://192.168.57.56/v1/workflows/run)         |
+-----------------------------------------------------------------------------------+
       |                                          ^
       | (Step 4: POST /api/writeback_table)       | (把修改确认后的数据带回)
       v                                          |
+-----------------------------------------------------------------------------------+
|             CAD 表格重构与旧实体擦除写入器 [CadTableWriter.cpp]                    |
|  - 方案 A: 根据句柄列表锁定文档并真实擦除旧实体 (pOldEnt->erase(true))             |
|  - 创建原生 ZcDbTable，计算总高度并与 bbox.min_y 底边对齐                           |
|  - 提交 ModelSpace 并强行刷新视口 (acedUpdateDisplay)                             |
+-----------------------------------------------------------------------------------+
```

---

## 3. 核心 API 接口说明

### 3.1 连通性检查 (`GET /api/status`)
- **功能**: 检测 C++ HTTP 服务连通性。
- **响应示例**:
  ```json
  { "status": "ok", "cad_version": "ZWCAD 2025", "plugin": "simple_table_prase.zrx" }
  ```

### 3.2 触发 CAD 视口框选与 AI 提取 (`POST /api/select_and_process`)
- **请求参数**:
  ```json
  { "convert_mode": 1 }  // 1: BOM表模式(8字段); 2: 标题栏模式(26字段)
  ```
- **核心逻辑**:
  1. 挂起并在 CAD 主 UI 线程中唤醒鼠标，引导用户在视口中点击两个角点进行**单次框选**。
  2. 自动提取选框范围内的被框选实体句柄（`selected_handles`）与选区物理坐标包围盒（`bbox`）。
  3. 调起 Dify 大模型 API，自动进行重试（最多 3 次，短超时防卡死），返回真实的提取 JSON。
- **响应示例 (BOM模式)**:
  ```json
  {
    "success": true,
    "convert_mode": 1,
    "bbox": { "min_x": 100.0, "min_y": 80.0, "max_x": 300.0, "max_y": 200.0 },
    "selected_handles": [ "1FA8", "1FA9" ],
    "extracted_fields": {
      "items": [
        {
          "serial_no": "1",
          "drawing_no": "ZRX-BOM-001",
          "name": "Guide Bush",
          "quantity": "2",
          "material": "Steel 45#",
          "unit_weight": "0.5",
          "total_weight": "1.0",
          "remark": "Standard Part"
        }
      ]
    }
  }
  ```

### 3.3 提交确认数据并生成原生表格 (`POST /api/writeback_table`)
- **请求参数**:
  ```json
  {
    "convert_mode": 1,
    "style_type": 1,
    "bbox": { "min_x": 100.0, "min_y": 80.0, "max_x": 300.0, "max_y": 200.0 },
    "fields": { "drawing_name": "确认后的名称", "quantity": "5" },
    "erase_handles": [ "1FA8", "1FA9" ]
  }
  ```
- **核心逻辑**:
  1. 打开 `acDocManager->lockDocument` 文档锁。
  2. 遍历 `erase_handles`，通过 `zcdbOpenObject` 真正将原始旧实体擦除 (`pOldEnt->erase(true)`)。
  3. 创建原生 `ZcDbTable`，根据总高度将插入点对齐于 `bbox.min_y` 底边。
  4. 追加至 ModelSpace，解锁文档，并强行刷新视口 (`acedUpdateDisplay`)。

---

## 4. 关键技术突破与死锁解决方案

### 4.1 跨线程 UI 交互死锁问题解决
- **痛点**: WinSock2 网络线程属于独立线程，直接调用 `acedGetCorner` 会与 CAD 主线程的消息循环产生跨线程死锁，导致鼠标在 CAD 中卡死无法移动。
- **解决方案**: 在 `CadSelectProcessor.cpp` 中引入 ZRX 全局应用程序上下文调度 API `acDocManager->executeInApplicationContext`。HTTP 线程发起 `std::promise` 挂起，任务投递给 CAD 主 UI 线程顺畅拉框，选完后再唤醒 HTTP 线程返回响应。

### 4.2 双重框选提示 Bug 解决
- **痛点**: 之前选完框后 CAD 控制台又弹出第二次 `选择对象:` 的二次提示。
- **解决方案**: 移除了无参 `GetTextEntitiesByUser` 的触发，直接使用第一次拉框产生的交叉选择集 `acedSSGet("C", pt1, pt2, ...)`，实现**一步框选到底**。

### 4.3 网络超时防挂起与 3 次重试
- **解决方案**: 在 `QuickDifyCall` 中设置 5 秒连接超时与 12 秒请求超时，重试 3 次失败后立即吐出标准 `200 OK` 错误 JSON，杜绝了无限挂起问题与保底假数据。

---

## 5. 项目工程源码目录结构

```
simple_zrx2025/
├── build_zrx.bat                    # MSBuild 自动化一键编译脚本
├── PROJECT_DELIVERY_DOCUMENTATION.md# 本项目交付文档
├── x64/Debug/
│   └── simple_table_prase.zrx       # 编译生成的 ZRX 插件二进制产物
└── ZrxDlgApp1/
    ├── CadSelectProcessor.h/cpp     # 视口单次框选、句柄收集与 Dify 调用核心模块
    ├── CadTableWriter.h/cpp         # 方案 A 旧实体擦除与原生 ZcDbTable 绘制模块
    ├── ZrxHttpServer.h/cpp          # WinSock2 轻量级 HTTP 服务器及 API 路由分发
    ├── AgentTableSum.h/cpp          # CAD 实体文本解析与后端通用基础库
    ├── rxentrypoint.cpp             # ZRX 插件入口注册与 AI_Convert 命令注册
    ├── Common.h/cpp                 # 字符串编码与跨平台通用工具集
    └── ZrxDlgApp1.vcxproj           # Visual Studio 2022 C++ 工程配置文件
```

---

## 6. 构建、安装与运行指南

### 6.1 编译工程
在 Windows PowerShell 或 MSBuild 命令行中运行：
```powershell
cd C:\Users\zwsoft\Desktop\transform
.\build_zrx.bat
```
编译成功后将输出：`simple_zrx2025/x64/Debug/simple_table_prase.zrx`。

### 6.2 在中望 CAD 2025 中加载与启动
1. 启动中望 CAD 2025 (ZWCAD 2025)。
2. 在 CAD 命令行输入 **`APPLOAD`** 或 **`ZRX`** 命令。
3. 浏览并选中 `simple_table_prase.zrx` 点击**加载**。
4. 在 CAD 命令行输入命令：**`AI_Convert`**。
5. 控制台输出 `[AI_Convert] Local HTTP Server running on http://127.0.0.1:18088` 即代表本地服务已就绪！

---

## 7. 交付产物与 Git 同步信息

- **Git 远程仓库**: `https://github.com/imbrucelee02-droid/zrx.git`
- **代码分支**: `master`
- **构建状态**: `0 错误`
