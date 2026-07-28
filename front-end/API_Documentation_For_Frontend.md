# CAD 网页端交互模块 C++ 后端 HTTP REST API 接口文档

本文档供前端开发同事接入 **ZWCAD C++ 插件 (`simple_table_prase.zrx`)** 本地 HTTP 服务使用。前端应用可通过标准的 HTTP REST API 与 CAD 后端进行双向交互。

---

## 1. 全局配置信息

- **Base URL**: `http://127.0.0.1:18088`
- **数据格式**: `JSON (application/json; charset=utf-8)`
- **跨域支持 (CORS)**: 后端已全量开启 CORS 头 (`Access-Control-Allow-Origin: *`)，支持前端直接跨域 Fetch/Axios 调用。
- **模式定义 (`convert_mode`)**:
  - `1`: **BOM 表转换模式**
  - `2`: **标题栏转换模式**

---

## 2. 接口列表

### 接口 1: 检查后端服务连通性 (Health Check)

用于前端初始化时检测 CAD C++ 后台 HTTP 服务是否已启动。

- **HTTP 方法**: `GET`
- **请求 Path**: `/api/status`
- **请求 Header**: 无
- **请求 Body**: 无

#### 响应示例 (`200 OK`)
```json
{
  "status": "ok",
  "cad_version": "ZWCAD 2025",
  "plugin": "simple_table_prase.zrx"
}
```

---

### 接口 2: Step 2 触发 CAD 框选并提取图纸数据

前端进入 Step 2 点击“激活 CAD 框选”时调用。此接口会挂起并唤醒 CAD 主视口让用户画框框选图纸区域，并返回提取到的字段数据、框选包围盒坐标 `bbox` 以及被框选实体的句柄列表 `selected_handles`。

- **HTTP 方法**: `POST`
- **请求 Path**: `/api/select_and_process`
- **请求 Header**:
  - `Content-Type`: `application/json`

#### 请求参数 (Request Body)
```json
{
  "convert_mode": 1
}
```

| 字段名 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| `convert_mode` | Integer | 是 | 转换模式：`1` 为 BOM 表模式，`2` 为标题栏模式 |

#### 响应示例 (`200 OK`)

##### 情况 A：当 `convert_mode = 1` (BOM 表模式) 时返回 8 字段结构列表
```json
{
  "success": true,
  "message": "Real CAD selection and processing completed",
  "convert_mode": 1,
  "bbox": {
    "min_x": 120.5,
    "min_y": 80.2,
    "max_x": 350.8,
    "max_y": 190.4
  },
  "selected_handles": [
    "1FA8",
    "1FA9",
    "1FAA"
  ],
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

##### 情况 B：当 `convert_mode = 2` (标题栏模式) 时返回 26 字段全集对象
```json
{
  "success": true,
  "message": "Real CAD selection and processing completed",
  "convert_mode": 2,
  "bbox": {
    "min_x": 120.5,
    "min_y": 80.2,
    "max_x": 350.8,
    "max_y": 190.4
  },
  "selected_handles": [
    "1FA8",
    "1FA9"
  ],
  "extracted_fields": {
    "enterprise_name": "ZWSOFT",
    "drawing_name": "Guide Bush Assembly",
    "drawing_no": "ZRX-2026-001",
    "product_or_material_mark": "Steel 45#",
    "weight": "1.5kg",
    "designer": "Designer A",
    "reviewer": "Reviewer B",
    "standardizer": "",
    "process_engineer": "",
    "drawing_date": "2026-07-23",
    "sheet_total": "1",
    "sheet_current": "1",
    "scale": "1:1",
    "drawing_sheet_count": "1",
    "sheet_size": "A4",
    "checker": "",
    "final_reviewer": "",
    "approver": "Manager C",
    "drawer": "Designer A",
    "assembly_name": "",
    "assembly_drawing_no": "",
    "unit_weight": "",
    "position_no": "",
    "quantity": "2",
    "revision_no": "",
    "remark": "Standard"
  }
}
```

> **前端开发注意**:
> `bbox` 与 `selected_handles` 属于控制参数，**无需在前端页面 UI 上展示给用户**。请在前端内存（如 `st.session_state` 或组件 state）中暂存它们，以便在 Step 4 调用回写接口时原封不动发回给 C++ 后端。

---

### 接口 3: Step 4 提交修改确认数据并回写 CAD 表格

前端在 Step 3 允许用户编辑修改字段及选择目标样式后，在 Step 4 点击“确认转换/回写”时调用。C++ 后端接收数据后，会在 CAD 模型空间生成原生 `ZcDbTable` 表格（底边与 `bbox.min_y` 对齐），同时**自动擦除 `erase_handles` 列表中的原始旧表格实体**。

- **HTTP 方法**: `POST`
- **请求 Path**: `/api/writeback_table`
- **请求 Header**:
  - `Content-Type`: `application/json`

#### 请求参数 (Request Body)
```json
{
  "convert_mode": 2,
  "style_type": 1,
  "bbox": {
    "min_x": 120.5,
    "min_y": 80.2,
    "max_x": 350.8,
    "max_y": 190.4
  },
  "fields": {
    "drawing_name": "导轮轴套 (修改确认后)",
    "drawing_no": "ZRX-2026-001",
    "material": "45# 钢",
    "quantity": "5"
  },
  "erase_handles": [
    "1FA8",
    "1FA9"
  ]
}
```

| 字段名 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| `convert_mode` | Integer | 是 | 转换模式：`1` 为 BOM 表模式，`2` 为标题栏模式 |
| `style_type` | Integer | 是 | 目标样式类型序号（例如 BOM 样式 1/2，标题栏 1~5） |
| `bbox` | Object | 是 | Step 2 获取到的包围盒坐标对象 |
| `fields` | Object | 是 | 用户在前端页面修改确认后的最终 key-value 数据 |
| `erase_handles` | Array[String] | 否 | 待擦除的原始旧实体句柄数组（直接传入 Step 2 返回的 `selected_handles`） |

#### 响应示例 (`200 OK`)
```json
{
  "success": true,
  "message": "ZcDbTable created successfully and old entities erased"
}
```

---

## 3. 标准字段定义规范 (Field Definitions)

### 3.1 标题栏 26 个标准字段 (`convert_mode = 2`)
| Key 键名 | 中文名称说明 | Key 键名 | 中文名称说明 |
| :--- | :--- | :--- | :--- |
| `enterprise_name` | 企业名称 | `drawing_name` | 图样名称 |
| `drawing_no` | 图样代号 | `product_or_material_mark` | 产品名称或材料标记 |
| `weight` | 重量 | `designer` | 设计 |
| `reviewer` | 审核 | `standardizer` | 标准化 |
| `process_engineer` | 工艺 | `drawing_date` | 日期 |
| `sheet_total` | 共几页 | `sheet_current` | 第几页 |
| `scale` | 比例 | `drawing_sheet_count` | 图纸张数 |
| `sheet_size` | 图幅 | `checker` | 校对 |
| `final_reviewer` | 审定 | `approver` | 批准 |
| `drawer` | 制图 | `assembly_name` | 装配名称 |
| `assembly_drawing_no` | 装配图号 | `unit_weight` | 单重 |
| `position_no` | 位号 | `quantity` | 数量 |
| `revision_no` | 制修号 | `remark` | 备注 |

### 3.2 BOM 表 8 个标准字段 (`convert_mode = 1`)
| Key 键名 | 中文名称说明 | Key 键名 | 中文名称说明 |
| :--- | :--- | :--- | :--- |
| `serial_no` | 序号 | `drawing_no` | 图号 / 代号 |
| `name` | 名称 | `quantity` | 数量 |
| `material` | 材料 | `unit_weight` | 单重 |
| `total_weight` | 总重 | `remark` | 备注 |

---

## 4. 前端调用示例 (JavaScript / Fetch)

```javascript
// Step 2: 触发 CAD 框选并获取数据
async function triggerCadSelection(mode = 1) {
  const res = await fetch('http://127.0.0.1:18088/api/select_and_process', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ convert_mode: mode })
  });
  const data = await res.json();
  
  if (data.success) {
    // 1. 隐藏暂存控制参数
    window.sessionStorage.setItem('cad_bbox', JSON.stringify(data.bbox));
    window.sessionStorage.setItem('cad_erase_handles', JSON.stringify(data.selected_handles));
    
    // 2. 将 extracted_fields 渲染至 UI 表格给用户编辑
    renderTableEditor(data.extracted_fields);
  }
}

// Step 4: 提交确认后的数据并生成表格
async function submitWriteback(confirmedFields, styleType = 1, mode = 1) {
  const bbox = JSON.parse(window.sessionStorage.getItem('cad_bbox'));
  const eraseHandles = JSON.parse(window.sessionStorage.getItem('cad_erase_handles'));

  const res = await fetch('http://127.0.0.1:18088/api/writeback_table', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      convert_mode: mode,
      style_type: styleType,
      bbox: bbox,
      fields: confirmedFields,
      erase_handles: eraseHandles
    })
  });
  const data = await res.json();
  alert(data.message);
}
```
