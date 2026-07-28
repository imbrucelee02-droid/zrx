import streamlit as st
import requests
import pandas as pd
import json


# ====================== 全局配置 ======================
CAD_API_BASE = "http://127.0.0.1:18088"
MODE_MAP = {"bom": 1, "titleblock": 2}
STYLE_MAP = {"样式1(图号)": 1, "样式2(代号)": 2,
              "标题栏-1(15字段)": 1, "标题栏-2(19字段)": 2,
              "标题栏-3(17字段)": 3, "标题栏-4(16字段)": 4, "标题栏-5(26字段)": 5}

FIELD_NAMES: dict[str, str] = {
    "enterprise_name": "企业名称",         "drawing_name": "图样名称",
    "drawing_no": "图样代号",             "product_or_material_mark": "产品名称或材料标记",
    "weight": "重量",                     "designer": "设计",
    "reviewer": "审核",                   "standardizer": "标准化",
    "process_engineer": "工艺",            "drawing_date": "日期",
    "sheet_total": "共几页",              "sheet_current": "第几页",
    "scale": "比例",                      "drawing_sheet_count": "图纸张数",
    "sheet_size": "图幅",                 "checker": "校对",
    "final_reviewer": "审定",             "approver": "批准",
    "drawer": "制图",                     "assembly_name": "装配名称",
    "assembly_drawing_no": "装配图号",     "unit_weight": "单重",
    "position_no": "位号",                "quantity": "数量",
    "revision_no": "制修号",              "remark": "备注",
}
BOM_FIELDS = {
    "serial_no": "序号", "drawing_no": "图号/代号", "name": "名称",
    "quantity": "数量", "material": "材料", "unit_weight": "单重",
    "total_weight": "总重", "remark": "备注",
}

BOM_STYLES = ["样式1(图号)", "样式2(代号)"]
TITLEBLOCK_STYLES = [
    "标题栏-1(15字段)", "标题栏-2(19字段)", "标题栏-3(17字段)",
    "标题栏-4(16字段)", "标题栏-5(26字段)",
]


# ====================== 会话状态初始化 ======================
def init_session():
    if "step" not in st.session_state:
        st.session_state.step = 1
    if "mode" not in st.session_state:
        st.session_state.mode = "titleblock"
    if "all_finished" not in st.session_state:
        st.session_state.all_finished = False


# ====================== API 工具函数 提供接口，对接口的修改可以在这里实现 ======================
def call_select_and_process(mode: str) -> dict:
    """Step 2: 触发CAD框选+提取。根据模式精准发往专属后端路由 /api/select_bom 或 /api/select_titleblock。"""
    endpoint = "/api/select_bom" if mode == "bom" else "/api/select_titleblock"
    payload = {"convert_mode": MODE_MAP[mode]}
    try:
        resp = requests.post(
            f"{CAD_API_BASE}{endpoint}",
            json=payload, headers={"Content-Type": "application/json"}, timeout=300,
        )
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.Timeout:
        return {"success": False, "message": "请求超时"}
    except requests.exceptions.ConnectionError:
        return {"success": False, "message": "CAD 服务未启动 (127.0.0.1:18088)"}
    except Exception as e:
        return {"success": False, "message": str(e)}


def call_writeback_table(payload: dict) -> dict:
    """Step 4: 回写 ZcDbTable。"""
    try:
        resp = requests.post(
            f"{CAD_API_BASE}/api/writeback_table",
            json=payload, headers={"Content-Type": "application/json"}, timeout=300,
        )
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.Timeout:
        return {"success": False, "message": "请求超时"}
    except requests.exceptions.ConnectionError:
        return {"success": False, "message": "CAD 服务未启动"}
    except Exception as e:
        return {"success": False, "message": str(e)}


def call_health_check() -> dict:
    """检查 CAD 后端服务连通性"""
    try:
        resp = requests.get(f"{CAD_API_BASE}/api/status", timeout=5)
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ConnectionError:
        return {"status": "error", "message": "CAD 服务未启动 (127.0.0.1:18088)"}
    except Exception as e:
        return {"status": "error", "message": str(e)}


# ====================== 样式分类引擎 ======================
def classify_style(data: dict, mode: str) -> str:
    """根据非空字段自动推荐样式。BOM: 有代号→样式2; 标题栏: 按扩展字段→校对→制图→日期优先级判定。"""
    if mode == "bom":
        return "样式2(代号)" if data.get("drawing_no", "").strip() else "样式1(图号)"

    has = lambda k: bool(data.get(k, "").strip())
    if any(has(k) for k in ["assembly_name", "position_no", "revision_no", "unit_weight", "remark"]):
        return "标题栏-5(26字段)"
    if has("checker") or has("approver"):
        return "标题栏-2(19字段)"
    if has("drawer") or has("final_reviewer"):
        return "标题栏-3(17字段)"
    if not has("drawing_date"):
        return "标题栏-4(16字段)"
    return "标题栏-1(15字段)"


def get_titleblock_fields(style: str) -> list[tuple[str, str]]:
    """按样式裁剪标题栏字段列表"""
    cut = {"标题栏-1(15字段)": 15, "标题栏-2(19字段)": 19, "标题栏-3(17字段)": 17,
           "标题栏-4(16字段)": 16, "标题栏-5(26字段)": 26}
    keys = list(FIELD_NAMES.keys())[:cut.get(style, 26)]
    return [(k, FIELD_NAMES[k]) for k in keys]


# ====================== UI 主题 ======================
def apply_cad_theme():
    st.markdown("""
    <style>
        .stApp { background-color: #2b3035; color: #f5f5f0; }
        header[data-testid="stHeader"] { background-color: #2b3035; border-bottom: 1px solid #1e2227; }
        .stApp h1,.stApp h2,.stApp h3,.stApp h4,.stApp p,.stApp li,.stApp span,.stApp label,
        .stApp div[data-testid="stMarkdownContainer"] { color: #fff !important; }
        .stApp h1 { color: #fff !important; font-weight: 600; }
        .stApp h2 { color: #fff !important; font-weight: 600; }
        .stApp h3 { color: #fff !important; font-weight: 500; }
        .stApp hr { border-color: #3a4149; opacity: 0.8; }
        .stApp button[kind="primary"] { background-color: #1f6fb2; color: #fff; border: 1px solid #165a91; font-weight: 500; }
        .stApp button[kind="primary"]:hover { background-color: #2a87d0; border-color: #1f6fb2; color: #fff; }
        .stApp button[kind="secondary"] { background-color: #2b3035; color: #fff; border: 1px solid #505a66; }
        .stApp button[kind="secondary"]:hover { background-color: #455060; border-color: #606c7a; color: #fff; }
        .stApp [data-testid="stProgress"]>div>div>div { background-color: #1f6fb2 !important; }
        .stApp [data-testid="stProgress"]>div>div { background-color: #1e2227 !important; }
        .stApp [data-testid="stAlert"] { background-color: #2a352a; border: 1px solid #3d5a3d; color: #a8d0a8; border-radius: 2px; }
        .stApp [data-testid="stAlert"] p,.stApp [data-testid="stAlert"] div { color: #a8d0a8 !important; }
        .stApp .stInfo { background-color: #273240; border: 1px solid #3a5570; color: #9ec0e0; border-radius: 2px; }
        .stApp .stInfo p,.stApp .stInfo div { color: #9ec0e0 !important; }
        .stApp .stWarning { background-color: #3a3320; border: 1px solid #6b5a28; color: #d4b87a; border-radius: 2px; }
        .stApp .stWarning p,.stApp .stWarning div { color: #d4b87a !important; }
        .stApp .stError { background-color: #3a2424; border: 1px solid #703838; color: #e09090; border-radius: 2px; }
        .stApp .stError p,.stApp .stError div { color: #e09090 !important; }
        .stApp [data-testid="stDataFrame"],.stApp [data-testid="stDataEditor"] { background-color: #000 !important; }
        .stApp [data-testid="stDataFrame"] table,.stApp [data-testid="stDataEditor"] table { background-color: #000 !important; color: #fff !important; }
        .stApp [data-testid="stDataFrame"] th,.stApp [data-testid="stDataEditor"] th { background-color: #1a1a1a !important; color: #fff !important; font-weight: 600; border-bottom: 1px solid #444; }
        .stApp [data-testid="stDataFrame"] td,.stApp [data-testid="stDataEditor"] td { color: #fff !important; border-bottom: 1px solid #333; }
        .stApp [data-testid="stDataFrame"] tr:hover td,.stApp [data-testid="stDataEditor"] tr:hover td { background-color: #222 !important; }
        .stApp [data-testid="stDataEditor"] input { color: #fff !important; background-color: #111 !important; }
        .stApp [data-testid="stSpinner"] svg circle { stroke: #1f6fb2 !important; }
        .stApp input,.stApp textarea,.stApp select { background-color: #23272c !important; color: #fff !important; border: 1px solid #3a4149 !important; }
        .stApp input:focus,.stApp textarea:focus { border-color: #1f6fb2 !important; }
        .stApp ::-webkit-scrollbar { width:10px; height:10px; }
        .stApp ::-webkit-scrollbar-track { background: #1e2227; }
        .stApp ::-webkit-scrollbar-thumb { background: #455060; border-radius: 2px; }
        .stApp ::-webkit-scrollbar-thumb:hover { background: #556070; }
        .stApp [data-testid="stMetric"] { background-color: #23272c; border: 1px solid #3a4149; border-radius: 4px; padding: 8px; }
        .stApp [data-testid="stMetric"] label,.stApp [data-testid="stMetric"] div { color: #fff !important; }
    </style>""", unsafe_allow_html=True)


# ====================== 步骤指示器 ======================
def render_step_indicators():
    names = ["① 模式选择", "② CAD 框选提取", "③ 识别结果确认", "④ 表格替换"]
    cur = st.session_state.step - 1
    cols = st.columns(4)
    for i, c in enumerate(cols):
        with c:
            if i < cur:       st.success(f"{names[i]} ✅")
            elif i == cur:    st.info(f"【进行中】{names[i]}")
            else:             st.caption(names[i])
    st.progress(min(cur / 4, 1.0))
    st.divider()


# ====================== Step 1 ======================
def render_step1(mode: str):
    label = "BOM 表" if mode == "bom" else "标题栏"
    st.header(f"第一步：{label}识别初始化")
    st.info(f"当前模式：**{label}转换**。系统将调用后端对图纸{label}进行智能提取。")
    if st.button("开始识别", type="primary", use_container_width=True):
        with st.spinner("正在检查 CAD 服务连接..."):
            health = call_health_check()
        if health.get("status") != "ok":
            st.error(f"服务不可用：{health.get('message', '未知错误')}")
            return
        st.success(f"✅ 服务连接成功 | {health.get('cad_version', '')}")
        st.session_state.step = 2
        st.session_state.has_triggered_select = False
        st.rerun()


import re


def clean_json_str(s):
    if not isinstance(s, str):
        return s
    s = s.strip()
    if s.startswith("```"):
        s = re.sub(r"^```[a-zA-Z]*\n?", "", s)
        s = re.sub(r"\n?```$", "", s)
        s = s.strip()
    return s


def unwrap_data(data):
    """递归解包多层嵌套 JSON 字符串或字典 (包含 data / outputs / output / items / extracted_fields 键，自动剥离 Markdown 代码块)"""
    for _ in range(10):
        if isinstance(data, dict):
            if "extracted_fields" in data:
                data = data["extracted_fields"]
            elif "outputs" in data:
                data = data["outputs"]
            elif "output" in data:
                data = data["output"]
            elif "items" in data:
                data = data["items"]
            elif "data" in data and isinstance(data["data"], (dict, str)):
                data = data["data"]
            else:
                break
        elif isinstance(data, str):
            cleaned = clean_json_str(data)
            try:
                data = json.loads(cleaned)
            except Exception:
                cleaned2 = cleaned.lstrip('\ufeff\ufffe').strip('\x00')
                try:
                    data = json.loads(cleaned2)
                except Exception:
                    break
        else:
            break
    return data


# ====================== Step 2 ======================
def render_step2(mode: str):
    label = "BOM 表" if mode == "bom" else "标题栏"
    st.header(f"第二步：{label}框选与提取")
    st.warning("1. 请切换到 ZWCAD 视口\n\n2. 鼠标拉框选择区域\n\n3. 系统自动完成提取")

    # 自动触发 CAD 框选与识别，无需用户二次点击按钮
    if "has_triggered_select" not in st.session_state or not st.session_state.has_triggered_select:
        st.session_state.has_triggered_select = True
        with st.spinner("🚀 正在激活 CAD 框选，请在中望 CAD 视口中拉框选择..."):
            resp = call_select_and_process(mode)

        if not resp.get("success"):
            st.error(f"❌ 请求失败: {resp.get('message', '未知错误')}")
            if st.button("重新框选", type="primary"):
                st.session_state.has_triggered_select = False
                st.rerun()
            return

        data_obj = resp.get("data", {})
        extracted = unwrap_data(data_obj)

        if not extracted or (isinstance(extracted, dict) and not any(extracted.values())):
            st.warning("⚠️ 接收到响应，但数据内容为空（可能是文件路径未匹配或框选超时）。")
            if st.button("重新框选", type="primary"):
                st.session_state.has_triggered_select = False
                st.rerun()
            return

        if mode == "titleblock":
            st.session_state.raw_result = extracted if isinstance(extracted, dict) else {}
        else:
            if isinstance(extracted, dict) and "items" in extracted:
                st.session_state.raw_result = extracted["items"]
            elif isinstance(extracted, list):
                st.session_state.raw_result = extracted
            else:
                st.session_state.raw_result = [extracted] if isinstance(extracted, dict) else []

        st.session_state.bbox = resp.get("bbox") or data_obj.get("bbox") or {"min_x": 0.0, "min_y": 0.0, "max_x": 100.0, "max_y": 100.0}
        st.session_state.erase_handles = resp.get("selected_handles") or data_obj.get("selected_handles") or []
        st.session_state.recommended_style = classify_style(
            extracted if isinstance(extracted, dict) else {},
            mode,
        )

        st.session_state.step = 3
        st.rerun()
    else:
        if st.button("重新框选提取", type="secondary", use_container_width=True):
            st.session_state.has_triggered_select = False
            st.rerun()


# ====================== Step 3 ======================
def render_step3(mode: str):
    label = "BOM 表" if mode == "bom" else "标题栏"
    st.header(f"第三步：{label}识别结果确认")

    raw_res = st.session_state.get("raw_result", {})

    styles = BOM_STYLES if mode == "bom" else TITLEBLOCK_STYLES
    recommended = st.session_state.get("recommended_style", styles[0])
    if recommended not in styles:
        recommended = styles[0]
    selected = st.selectbox("输出样式", options=styles, index=styles.index(recommended))
    st.session_state.selected_style = selected

    st.subheader("数据编辑")
    if mode == "bom":
        edited = _render_bom_editor(raw_res)
    else:
        edited = _render_titleblock_editor(raw_res, selected)

    if st.button("确认结果，替换表格", type="primary", use_container_width=True):
        st.session_state.final_data = edited
        st.session_state.step = 4
        st.rerun()


BOM_KEY_ALIASES = {
    "serial_no": ["serial_no", "serialNo", "no", "序号", "xu_hao", "index"],
    "drawing_no": ["drawing_no", "drawingNo", "code", "图号", "代号", "图号/代号", "tu_hao", "dai_hao", "drawing_code"],
    "name": ["name", "itemName", "名称", "零件名称", "物料名称", "ming_cheng"],
    "quantity": ["quantity", "qty", "count", "数量", "num", "shu_liang"],
    "material": ["material", "mat", "材料", "材质", "cai_liao"],
    "unit_weight": ["unit_weight", "unitWeight", "单重", "dan_zhong"],
    "total_weight": ["total_weight", "totalWeight", "总重", "zong_zhong"],
    "remark": ["remark", "note", "备注", "bei_zhu"]
}


def extract_bom_item_value(it_dict, std_key):
    aliases = BOM_KEY_ALIASES.get(std_key, [std_key])
    for a in aliases:
        if a in it_dict and str(it_dict[a]).strip():
            return str(it_dict[a]).strip()
    return ""


def _render_bom_editor(items) -> list[dict]:
    """BOM 模式：多行物料表格编辑 (支持全量中英文及变体 Key 自动映射与 8 大标准列)"""
    st.caption("双击编辑单元格，末行可新增行，支持拖拽列宽")
    if isinstance(items, str):
        try:
            items = json.loads(items)
        except Exception:
            items = []

    col_order = ["serial_no", "drawing_no", "name", "quantity",
                 "material", "unit_weight", "total_weight", "remark"]

    if isinstance(items, dict):
        if "items" in items and isinstance(items["items"], list):
            items = items["items"]
        elif "extracted_fields" in items:
            items = items["extracted_fields"]
            if isinstance(items, dict) and "items" in items:
                items = items["items"]
            elif isinstance(items, dict) and all(isinstance(v, dict) for v in items.values()):
                items = list(items.values())
            else:
                items = [items] if isinstance(items, dict) else []
        elif all(isinstance(v, dict) for v in items.values()):
            items = list(items.values())
        else:
            items = [items]
    elif not isinstance(items, list):
        items = []

    normalized_items = []
    if isinstance(items, list) and items:
        for it in items:
            if isinstance(it, dict):
                row = {c: extract_bom_item_value(it, c) for c in col_order}
                # Keep extra keys that are not in default aliases
                all_aliases = {a for aliases in BOM_KEY_ALIASES.values() for a in aliases}
                for k, v in it.items():
                    if k not in all_aliases and v and str(v).strip():
                        row[k] = str(v).strip()
                normalized_items.append(row)

    if not normalized_items:
        normalized_items = [{c: "" for c in col_order}]

    df = pd.DataFrame(normalized_items)
    cols_in_df = [c for c in col_order if c in df.columns] + [c for c in df.columns if c not in col_order]
    df = df[cols_in_df]
    df.columns = [BOM_FIELDS.get(c, c) for c in df.columns]

    edited_df = st.data_editor(
        df,
        num_rows="fixed",
        use_container_width=True,
        column_config={
            "序号": st.column_config.TextColumn("序号", width="small"),
            "图号/代号": st.column_config.TextColumn("图号/代号", width="medium"),
            "名称": st.column_config.TextColumn("名称", width="large"),
            "数量": st.column_config.TextColumn("数量", width="small"),
            "材料": st.column_config.TextColumn("材料", width="medium"),
            "单重": st.column_config.TextColumn("单重", width="small"),
            "总重": st.column_config.TextColumn("总重", width="small"),
            "备注": st.column_config.TextColumn("备注", width="medium"),
        }
    )

    reverse = {v: k for k, v in BOM_FIELDS.items()}
    edited_df.columns = [reverse.get(c, c) for c in edited_df.columns]
    return edited_df.to_dict("records")


def _render_titleblock_editor(fields, style: str) -> list[dict]:
    """标题栏模式：键值对编辑"""
    if isinstance(fields, str):
        try:
            fields = json.loads(fields)
        except Exception:
            fields = {}
    if isinstance(fields, list):
        fields = fields[0] if fields and isinstance(fields[0], dict) else {}
    if not isinstance(fields, dict):
        fields = {}

    rows = [{"字段名": label, "识别值": str(fields.get(key, ""))}
            for key, label in get_titleblock_fields(style)]

    # Also append any recognized fields in data that are not in default style
    known_keys = {key for key, _ in get_titleblock_fields(style)}
    for k, v in fields.items():
        if k not in known_keys and v and str(v).strip():
            rows.append({"字段名": str(k), "识别值": str(v)})

    df = pd.DataFrame(rows)
    edited = st.data_editor(
        df, num_rows="fixed", use_container_width=True,
        column_config={"字段名": st.column_config.TextColumn(disabled=False)},
    )
    return edited.to_dict("records")


def build_writeback_fields(final_data, raw_result, mode):
    """根据 Step 3 用户编辑结果或 raw_result 兜底拼装回写数据 payload (保留选中排版样式的全部字段)"""
    fields_payload = {}
    target = final_data if (final_data and len(final_data) > 0) else raw_result

    if mode == "bom":
        if isinstance(target, list):
            fields_payload = {"items": target}
        elif isinstance(target, dict):
            if "items" in target:
                fields_payload = target
            else:
                fields_payload = {"items": [target]}
    else:
        # TitleBlock 模式：兼容 字段名/识别值 列表、扁平字典、多属性字典
        if isinstance(target, list):
            for r in target:
                if isinstance(r, dict):
                    if "字段名" in r and "识别值" in r:
                        fname = str(r.get("字段名", "")).strip()
                        fval = str(r.get("识别值", "")).strip()
                        if fname:
                            fields_payload[fname] = fval
                    else:
                        for k, v in r.items():
                            zh_name = FIELD_NAMES.get(k, k)
                            fields_payload[zh_name] = str(v).strip()
        elif isinstance(target, dict):
            if "items" in target and isinstance(target["items"], list):
                for r in target["items"]:
                    if isinstance(r, dict):
                        for k, v in r.items():
                            zh_name = FIELD_NAMES.get(k, k)
                            fields_payload[zh_name] = str(v).strip()
            else:
                for k, v in target.items():
                    zh_name = FIELD_NAMES.get(k, k)
                    fields_payload[zh_name] = str(v).strip()

        if not fields_payload and raw_result and isinstance(raw_result, dict):
            for k, v in raw_result.items():
                zh_name = FIELD_NAMES.get(k, k)
                fields_payload[zh_name] = str(v).strip()

    return fields_payload


# ====================== Step 4 ======================
def render_step4(mode: str):
    label = "BOM 表" if mode == "bom" else "标题栏"
    st.header(f"第四步：{label}表格替换")

    final = st.session_state.get("final_data", [])
    raw_res = st.session_state.get("raw_result", {})
    style_name = st.session_state.get("selected_style", "")
    bbox = st.session_state.get("bbox", {})

    c1, c2, c3 = st.columns(3)
    c1.metric("模式", label)
    c2.metric("样式", style_name)
    c3.metric("行数/字段数", len(final) if final else len(raw_res))

    fields_payload = build_writeback_fields(final, raw_res, mode)

    payload = {
        "convert_mode": MODE_MAP[mode],
        "style_type": STYLE_MAP.get(style_name, 1),
        "bbox": bbox,
        "erase_handles": st.session_state.get("erase_handles", []),
        "fields": fields_payload,
        "fields_data": fields_payload
    }


    if st.button("表格替换", type="primary", use_container_width=True):
        with st.spinner("正在生成 ZcDbTable..."):
            result = call_writeback_table(payload)

        if result.get("success"):
            st.success("✅ 表格替换成功！旧表格已清空，新表格已原位放置完毕")
            st.session_state.all_finished = True
        else:
            st.error(f"回写失败：{result.get('message')}")

    if st.session_state.get("all_finished"):
        st.success("🎉 全部流程完成！可点击下方按钮重置")


def on_mode_change():
    selected_choice = st.session_state.get("mode_selector_widget")
    new_mode = "bom" if selected_choice == "BOM 表转换" else "titleblock"
    if st.session_state.get("mode") != new_mode:
        st.session_state.mode = new_mode
        st.session_state.step = 1
        st.session_state.has_triggered_select = False
        st.session_state.final_data = []
        st.session_state.raw_result = {}


# ====================== 主函数 ======================
def main():
    st.set_page_config(page_title="CAD机械表格标准化平台", page_icon="🏗️", layout="wide")
    init_session()
    apply_cad_theme()

    st.title("CAD机械表格标准化平台")

    if "mode_selector_widget" not in st.session_state:
        st.session_state["mode_selector_widget"] = "BOM 表转换" if st.session_state.get("mode") == "bom" else "标题栏转换"

    choice = st.radio(
        "转换模式", ["标题栏转换", "BOM 表转换"],
        horizontal=True, label_visibility="collapsed",
        key="mode_selector_widget",
        on_change=on_mode_change
    )
    st.session_state.mode = "bom" if choice == "BOM 表转换" else "titleblock"
    mode = st.session_state.mode

    st.divider()
    render_step_indicators()
    st.subheader("功能操作区")

    {"1": render_step1, "2": render_step2, "3": render_step3, "4": render_step4
     }.get(str(st.session_state.step), render_step1)(mode)

    st.divider()
    if st.button("重置全部流程，重新开始", type="secondary"):
        for k in list(st.session_state.keys()):
            del st.session_state[k]
        st.rerun()


if __name__ == "__main__":
    main()
