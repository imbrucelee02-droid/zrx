# 中望 CAD (ZWCAD 2025) AI 图纸识别与重构插件 - 项目最终交付与交接文档

## 1. 项目概述与架构设计

本项目基于中望 CAD 2025 ZRX SDK 开发，为一个高效的 C++ 动态链接库插件（`simple_table_prase.zrx`）。

插件内部嵌入了一个基于 WinSock2 的轻量级 HTTP REST API 服务器（默认监听 `http://127.0.0.1:18088`），实现了外部 Web 前端（Streamlit / Vue / React）与 CAD 绘图引擎的高效无缝解耦交互。

### 核心架构图
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
|             CAD 视口真实交互与 AI 命令 [CadSelectProcessor.cpp]                     |
|  - 视口单次画框选择 (acedGetCorner)                                                |
|  - 交叉选择集收集实体 Handle 句柄 (acedSSGet "C")                                  |
|  - 100% 调起原生 AiBomConvertCmd / AiTableRecognizeCmd                             |
|  - 调起在线 Dify 大模型 API 工作流 (http://192.168.57.56/v1/workflows/run)         |
+-----------------------------------------------------------------------------------+
       |                                          ^
       | (Step 4: POST /api/writeback_table)       | (把修改确认后的数据带回)
       v                                          |
+-----------------------------------------------------------------------------------+
|             CAD 表格重构与旧实体擦除写入器 [CadTableWriter.cpp]                    |
|  - 方案 A: 锁定文档 (lockDocument) 并物理擦除旧实体 (pOldEnt->erase(true))         |
|  - 创建原生 ZcDbTable，计算总高度与包围盒底边对齐 (bbox.min_y)                      |
|  - 提交 ModelSpace 并强行刷新图形视口 (acedUpdateDisplay)                          |
+-----------------------------------------------------------------------------------+
```

---

## 2. 核心 REST API 规范

### 2.1 心跳与连通性检查
- **请求**: `GET http://127.0.0.1:18088/api/status`
- **响应示例**:
  ```json
  {
    "status": "ok",
    "cad_version": "ZWCAD 2025",
    "plugin": "simple_table_prase.zrx"
  }
  ```

### 2.2 触发 CAD 视口框选与 AI 识别
- **请求**: `POST http://127.0.0.1:18088/api/select_and_process`
- **Header**: `Content-Type: application/json`
- **Body**:
  ```json
  {
    "convert_mode": 0
  }
  ```
  *说明*: `convert_mode`: `0` 或 `2` 代表标题栏模式（26 字段）；`1` 代表 BOM 表模式（8 字段）。

- **响应示例**:
  ```json
  {
    "success": true,
    "convert_mode": 0,
    "message": "Real CAD selection and Dify processing completed",
    "bbox": { "min_x": 425.7, "min_y": 114.0, "max_x": 584.0, "max_y": 199.1 },
    "selected_handles": [ "1FA8", "1FA9" ],
    "extracted_fields": {
      "enterprise_name": "深圳市沃尔新能源电气科技股份有限公司",
      "drawing_name": "直角插头250A总成",
      "drawing_no": "68051037993 DWG NO.",
      "sheet_size": "A4",
      "revision_no": "AO"
    }
  }
  ```

### 2.3 提交确认数据并写回原生表格
- **请求**: `POST http://127.0.0.1:18088/api/writeback_table`
- **Header**: `Content-Type: application/json`
- **Body**:
  ```json
  {
    "convert_mode": 0,
    "style_type": 1,
    "bbox": { "min_x": 425.7, "min_y": 114.0, "max_x": 584.0, "max_y": 199.1 },
    "fields": {
      "drawing_name": "确认修改后的图纸名称",
      "drawing_no": "68051037993"
    },
    "erase_handles": [ "1FA8", "1FA9" ]
  }
  ```
- **响应示例**:
  ```json
  {
    "success": true,
    "message": "ZcDbTable created successfully and old entities erased"
  }
  ```

---

## 3. 关键技术解决方案与坑点记录

### 3.1 跨线程 UI 交互死锁
- **问题**: HTTP 接口是在 WinSock2 后台线程运行的，直接调用 CAD 交互函数（如 `acedGetPoint` / `acedGetCorner`）会导致 CAD UI 线程与网络线程死锁，表现为鼠标冻结卡死。
- **解决方案**: 使用 ZRX SDK 全局调度 `acDocManager->executeInApplicationContext` 将交互推送到 CAD 主线程，结合 `std::promise`/`std::future` 挂起和唤醒 HTTP 线程。

### 3.2 代理网卡拦截挂起
- **问题**: 当本机开启了 TUN / Mihomo 代理网卡时，cURL 发往局域网 Dify (`192.168.57.56`) 的请求会被代理网卡拦截，导致接口无限挂起超时。
- **解决方案**: 在 `QuickDifyCall` 的 cURL 配置中加入 `curl_easy_setopt(curl, CURLOPT_NOPROXY, "*")` 双重保险，彻底绕过代理直连。

### 3.3 物理实体擦除与文档锁
- **问题**: 在非 UI 主线程或修改 ModelSpace 时直接调用实体 `erase` 会报 eNotOpenForWrite 错误或图形残留。
- **解决方案**: 写入前开启 `acDocManager->lockDocument` 锁住文档；擦除旧实体调用 `pOldEnt->erase(true)`；在写入 `ZcDbTable` 后调用 `acedUpdateDisplay()` 强行刷新视口显示。

---

## 4. 源码目录与编译指南

### 4.1 目录结构
```
simple_zrx2025/
├── build_zrx.bat                    # 一键 MSBuild 编译脚本
├── PROJECT_DELIVERY_DOCUMENTATION.md# 详细项目交付文档
├── PROJECT_FINAL_HANDOVER.md       # 本项目交接指南
├── x64/Debug/
│   └── simple_table_prase.zrx       # 编译输出的二进制插件产物
└── ZrxDlgApp1/
    ├── CadSelectProcessor.h/cpp     # 视口单次框选与 Dify 调起模块
    ├── CadTableWriter.h/cpp         # 方案 A 旧实体擦除与原生 ZcDbTable 生成器
    ├── ZrxHttpServer.h/cpp          # WinSock2 轻量级 HTTP API 路由器
    ├── AgentTableSum.h/cpp          # 通用 CAD 图形算法与 Dify API 接口库
    ├── rxentrypoint.cpp             # ZRX 入口与 AI_Convert 命令注册
    └── ZrxDlgApp1.vcxproj           # Visual Studio 2022 C++ 工程
```

### 4.2 一键编译命令
在 `simple_zrx2025` 根目录运行：
```powershell
.\build_zrx.bat
```
*(注意：编译前请确保中望 CAD 中已 APPLOAD 卸载旧插件，否则可能报 LNK1168 占用错误)*。

---

## 5. 项目 Git 仓与交付信息

- **Git 远程仓库**: `https://github.com/imbrucelee02-droid/zrx.git`
- **默认分支**: `master`
- **编译状态**: `0 错误`
