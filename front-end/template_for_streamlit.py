import streamlit as st
import requests
import json
import os
import time
import pandas as pd
from io import BytesIO
from datetime import datetime
from conect_postgrep import get_taskid_db_tablesum, check_task_exists
import asyncio


# ====================== 全局配置 ======================
TABLE_OCR_API = "http://127.0.0.1:8080/table_sum_reconize"
TABLE_INTERACT = "http://127.0.0.1:8080/table_sum_interact"
TABLE_SUMMARY_API = "http://127.0.0.1:8080/table_sum_aitablesum"
TABLE_CHECK_ONCOMMAND_API = "http://127.0.0.1:8080/table_sum_check_oncommand"  #检查是否在命令中，防止交互没有完成就进行其他操作.
TABLE_ZOOM_API = "http://127.0.0.1:8080/table_sum_zoom"

#USE_TEST_QUICK = True  #True: 利用缓存结果；False: 从头开始识别,调api耗费token;
USE_TEST_QUICK = False 

# ====================== 会话状态初始化 ======================
def init_session():
    """初始化 Streamlit 会话状态"""
    if "step" not in st.session_state:
        st.session_state.step = 1
    if "table_edit_idx" not in st.session_state:
        st.session_state.table_edit_idx = 0
    if "edited_table_list" not in st.session_state:
        st.session_state.edited_table_list = []
    if "edited_table_set" not in st.session_state:
        st.session_state.edited_table_set = set()
    if "all_finished" not in st.session_state:
        st.session_state.all_finished = False

# ====================== API工具函数 ======================
def call_table_ocr_api():
    """调用表格识别API"""

    time.sleep(1)
    return {
        "code": 0,
        "msg": "表格识别接口调用成功"
    }

    if USE_TEST_QUICK:
        time.sleep(2)
        return {
            "code": 0,
            "msg": "表格识别接口调用成功"
        }

    #headers = {"Authorization": "Bearer your_token", "Content-Type": "application/json"}
    headers = {"Content-Type": "application/json"}
    payload = {
        "source": "cad_cache",
        "task_id": "auto_cad_table"
    }
    try:
        resp = requests.post(TABLE_OCR_API, json=payload, headers=headers, timeout=300)
        resp.raise_for_status()
        return resp.json()
    except Exception as e:
        return {"code": -1, "msg": f"表格识别接口调用失败：{str(e)}", "data": None}


def call_table_interact_api():
    """调用表格识别API"""
    if USE_TEST_QUICK:
        time.sleep(2)
        return {
            "code": 0,
            "msg": "表格交互识别调用成功"
        }

    #headers = {"Authorization": "Bearer your_token", "Content-Type": "application/json"}
    headers = {"Content-Type": "application/json"}
    payload = {
        "source": "cad_cache",
        "task_id": "auto_cad_table"
    }
    try:
        resp = requests.post(TABLE_INTERACT, json=payload, headers=headers, timeout=1)
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ReadTimeout:
        return {"code": -2, "msg": f"表格交互识别接口调用失败：超时", "data": None}
    except Exception as e:
        return {"code": -1, "msg": f"表格交互识别接口调用失败：{str(e)}", "data": None}


def call_table_summary_api():
    """调用表格汇总API"""
    if USE_TEST_QUICK:
        time.sleep(2)
        return {
            "code": 0,
            "msg": "汇总成功",
        }
    

    payload = {
    }

    #headers = {"Content-Type": "application/json", "Authorization": "Bearer your_token"}
    headers = {"Content-Type": "application/json"}
    try:
        resp = requests.post(TABLE_SUMMARY_API, json=payload, headers=headers, timeout=300)
        resp.raise_for_status()
        return resp.json()
    except Exception as e:
        return {"code": -1, "msg": f"汇总接口调用失败：{str(e)}", "data": None}


def call_table_check_oncommand_api():
    if USE_TEST_QUICK:
        return {
            "code": 0,
            "msg": "check状态成功",
        }

    headers = {"Content-Type": "application/json"}
    try:
        resp = requests.post(TABLE_CHECK_ONCOMMAND_API, headers=headers, timeout=1)
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ReadTimeout:
        return {"code": -2, "msg": f"超时", "data": None}
    except Exception as e:
        return {"code": -1, "msg": f"汇总接口调用失败：{str(e)}", "data": None}
    

def call_table_zoom_api(minRegionPt, maxRegionPt):
    try:
        headers = {"Content-Type": "application/json"}
        payload = {
            "minRegionPt": minRegionPt,
            "maxRegionPt": maxRegionPt
        }
    
        resp = requests.post(TABLE_ZOOM_API, json = payload, headers=headers, timeout=2)
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ReadTimeout:
        return {"code": -2, "msg": f"超时", "data": None}
    except Exception as e:
        return {"code": -1, "msg": f"汇总zoom api调用失败：{str(e)}", "data": None}


def check_cad_finish():
    """检查CAD操作是否完成"""
    if USE_TEST_QUICK:
        time.sleep(1)
        return True

    return check_task_exists("agent_tablesum_modified_taskid_001")

#将每个表导出到一个sheet
# def generate_excel(all_tables):
#     """生成Excel文件"""
#     buf = BytesIO()
#     with pd.ExcelWriter(buf, engine="openpyxl") as writer:
#         for idx, tb in enumerate(all_tables):
#             df = pd.DataFrame(tb["final_rows"])
#             #sheet_name = tb["table_name"][:20]
#             sheet_name = str(idx)
#             df.to_excel(writer, sheet_name=sheet_name, index=False)
#     buf.seek(0)
#     return buf


def generate_excel(all_tables):
    buf = BytesIO()
    df_detail, df_stat = merge_and_stat_tables(all_tables)
    with pd.ExcelWriter(buf, engine="openpyxl") as writer:
        #df_detail.to_excel(writer, sheet_name="全部明细", index=False) #如果要打开明细，此时没有合并
        if not df_stat.empty:
            df_stat.to_excel(writer, sheet_name="物料汇总统计", index=False)

    buf.seek(0)
    return buf

# ====================== UI主题配置 ======================
def apply_cad_theme():
    """应用CAD风格主题"""
    cad_theme_css = """
    <style>
        /* ===== 全局背景：CAD 深灰蓝画布 ===== */
        .stApp {
            background-color: #2b3035;
            color: #f5f5f0;
        }

        /* ===== 顶部标题栏：CAD 深灰菜单条 ===== */
        header[data-testid="stHeader"] {
            background-color: #2b3035;
            border-bottom: 1px solid #1e2227;
        }

        /* ===== 主文字颜色 全部强制纯白色（修复文字发暗问题！）===== */
        .stApp h1, .stApp h2, .stApp h3, .stApp h4,
        .stApp p, .stApp li, .stApp span, .stApp label,
        .stApp div[data-testid="stMarkdownContainer"] {
            color: #ffffff !important;
        }

        /* 标题加粗提亮，纯白 */
        .stApp h1 { color: #ffffff !important; font-weight: 600; }
        .stApp h2 { color: #ffffff !important; font-weight: 600; }
        .stApp h3 { color: #ffffff !important; font-weight: 500; }

        /* ===== 分割线：CAD 暗边框风格 ===== */
        .stApp hr {
            border-color: #3a4149;
            opacity: 0.8;
        }

        /* ===== 主按钮：蓝色主操作 ===== */
        .stApp button[kind="primary"] {
            background-color: #1f6fb2;
            color: #ffffff;
            border: 1px solid #165a91;
            font-weight: 500;
            transition: all 0.2s;
        }
        .stApp button[kind="primary"]:hover {
            background-color: #2a87d0;
            border-color: #1f6fb2;
            color: #ffffff;
        }
        .stApp button[kind="primary"]:active {
            background-color: #165a91;
        }

        /* ===== 次按钮文字改为纯白 ===== */
        .stApp button[kind="secondary"] {
            background-color: #2b3035;
            color: #ffffff;
            border: 1px solid #505a66;
        }
        .stApp button[kind="secondary"]:hover {
            background-color: #455060;
            border-color: #606c7a;
            color: #ffffff;
        }

        /* ===== 下载按钮同步蓝色主色调 ===== */
        .stApp [data-testid="stDownloadButton"] button {
            background-color: #1f6fb2;
            color: #ffffff;
            border: 1px solid #165a91;
            font-weight: 500;
        }
        .stApp [data-testid="stDownloadButton"] button:hover {
            background-color: #2a87d0;
            border-color: #1f6fb2;
            color: #ffffff;
        }

        /* ===== 进度条蓝色 ===== */
        .stApp [data-testid="stProgress"] > div > div > div {
            background-color: #1f6fb2 !important;
        }
        .stApp [data-testid="stProgress"] > div > div {
            background-color: #1e2227 !important;
        }

        /* ===== 成功提示：CAD 暗绿风格 ===== */
        .stApp [data-testid="stAlert"] {
            background-color: #2a352a;
            border: 1px solid #3d5a3d;
            color: #a8d0a8;
            border-radius: 2px;
        }
        .stApp [data-testid="stAlert"] p,
        .stApp [data-testid="stAlert"] div {
            color: #a8d0a8 !important;
        }

        /* ===== 信息提示：CAD 暗蓝风格 ===== */
        .stApp .stInfo {
            background-color: #273240;
            border: 1px solid #3a5570;
            color: #9ec0e0;
            border-radius: 2px;
        }
        .stApp .stInfo p,
        .stApp .stInfo div {
            color: #9ec0e0 !important;
        }

        /* ===== 警告提示：CAD 暗黄风格 ===== */
        .stApp .stWarning {
            background-color: #3a3320;
            border: 1px solid #6b5a28;
            color: #d4b87a;
            border-radius: 2px;
        }
        .stApp .stWarning p,
        .stApp .stWarning div {
            color: #d4b87a !important;
        }

        /* ===== 错误提示：CAD 暗红风格 ===== */
        .stApp .stError {
            background-color: #3a2424;
            border: 1px solid #703838;
            color: #e09090;
            border-radius: 2px;
        }
        .stApp .stError p,
        .stApp .stError div {
            color: #e09090 !important;
        }

        /* ===== 数据表格：纯黑底色 + 白色文字 ===== */
        .stApp [data-testid="stDataFrame"],
        .stApp [data-testid="stDataEditor"] {
            background-color: #000000 !important;
        }
        .stApp [data-testid="stDataFrame"] table,
        .stApp [data-testid="stDataEditor"] table {
            background-color: #000000 !important;
            color: #ffffff !important;
        }
        .stApp [data-testid="stDataFrame"] th,
        .stApp [data-testid="stDataEditor"] th {
            background-color: #1a1a1a !important;
            color: #ffffff !important;
            font-weight: 600;
            border-bottom: 1px solid #444444;
        }
        .stApp [data-testid="stDataFrame"] td,
        .stApp [data-testid="stDataEditor"] td {
            color: #ffffff !important;
            border-bottom: 1px solid #333333;
        }
        .stApp [data-testid="stDataFrame"] tr:hover td,
        .stApp [data-testid="stDataEditor"] tr:hover td {
            background-color: #222222 !important;
        }
        .stApp [data-testid="stDataEditor"] input {
            color: #ffffff !important;
            background-color: #111111 !important;
        }

        /* ===== Spinner 加载：蓝色 ===== */
        .stApp [data-testid="stSpinner"] svg circle {
            stroke: #1f6fb2 !important;
        }

        /* ===== 侧边栏（如有）：CAD 属性面板深灰 ===== */
        .stApp [data-testid="stSidebar"] {
            background-color: #25282c;
            border-right: 1px solid #1a1d20;
        }

        /* ===== 输入框文字纯白 ===== */
        .stApp input, .stApp textarea, .stApp select {
            background-color: #23272c !important;
            color: #ffffff !important;
            border: 1px solid #3a4149 !important;
        }
        .stApp input:focus, .stApp textarea:focus {
            border-color: #1f6fb2 !important;
        }

        /* ===== 代码块：命令行风格 ===== */
        .stApp code, .stApp pre {
            background-color: #1e2227 !important;
            color: #b5c88a !important;
            border: 1px solid #333940;
        }

        /* ===== 滚动条：深色 ===== */
        .stApp ::-webkit-scrollbar {
            width: 10px;
            height: 10px;
        }
        .stApp ::-webkit-scrollbar-track {
            background: #1e2227;
        }
        .stApp ::-webkit-scrollbar-thumb {
            background: #455060;
            border-radius: 2px;
        }
        .stApp ::-webkit-scrollbar-thumb:hover {
            background: #556070;
        }

        /* ===== 胶囊式步骤节点文字纯白 ===== */
        .stApp [data-testid="stMetric"] {
            background-color: #23272c;
            border: 1px solid #3a4149;
            border-radius: 4px;
            padding: 8px;
        }
        .stApp [data-testid="stMetric"] label,
        .stApp [data-testid="stMetric"] div {
            color: #ffffff !important;
        }
    </style>
    """
    st.markdown(cad_theme_css, unsafe_allow_html=True)

# ====================== 步骤界面函数 ======================
def render_step_indicators():
    """渲染步骤指示器"""
    step_names = [
        "① 图纸表格识别",
        "② CAD交互操作",
        "③ 表格归纳AI处理",
        "④ 汇总表格逐张确认",
        "⑤ 导出Excel报表"
    ]
    current_step_idx = st.session_state.step - 1
    total_step = 5

    cols = st.columns(5)
    for idx, col in enumerate(cols):
        with col:
            if st.session_state.all_finished:
                st.success(f"{step_names[idx]} ✅")
            else:
                if idx < current_step_idx:
                    st.success(f"{step_names[idx]} ✅")
                elif idx == current_step_idx:
                    st.info(f"【进行中】{step_names[idx]}")
                else:
                    st.caption(f"{step_names[idx]}")

    if st.session_state.all_finished:
        progress_rate = 1.0
    else:
        progress_rate = current_step_idx / total_step
    st.progress(progress_rate)
    st.divider()


def render_step1():
    """步骤1：表格识别"""
    st.header("第一步：图纸表格识别")
    st.info("操作指引：点击下方按钮调用外部识别API，系统读取当前图纸进行表格提取")
    # st.warning("""
    # 操作指引：
    # 1. 切换至本地CAD软件，请遵循命令流程。
    # 2. 当前图纸框选图纸范围进行表格识别。
    # 3. 完成后点击按钮确认CAD操作完成。
    # """)

    run_recognize = st.button("开始执行表格识别", type="primary", use_container_width=True)
    if run_recognize:
        with st.spinner("正在请求表格识别API，解析图纸表格..."):
            ocr_result = call_table_ocr_api()
        if ocr_result.get("code") == 0:
            st.success("表格识别完成！顶部进度节点将自动切换至第二步")
            st.session_state.step = 2

            with st.spinner("正在请求交互识别API"):
                interact_result = call_table_interact_api()
            if interact_result.get("code") == 0 or interact_result.get("code") == -2:
                st.rerun()
            else:
                st.error(f"交互失败：{interact_result.get('msg')}, {interact_result.get('code')}")
        else:
            st.error(f"识别失败：{ocr_result.get('msg')}")

    st.divider()
    if st.button("重置全部流程，重新开始", type="secondary"):
        for k in list(st.session_state.keys()):
            del st.session_state[k]
        st.rerun()

def render_step2():
    """步骤2：CAD交互检查"""
    st.header("第二步：CAD识别结果检查")
    st.warning("""
    操作指引：
    1. 切换至本地CAD软件，请遵循命令流程。
    2. 进行错误表格删除，漏掉表格重新框选识别。
    3. 完成后点击按钮确认CAD操作完成。
    """)

    refresh_btn = st.button("确认完成CAD识别结果检查", type="primary", use_container_width=True)
    if refresh_btn:
        check_res = call_table_check_oncommand_api()
        if check_res.get("code") == 1 or check_res.get("code") == -2: #在命令中
            st.error(f"操作失败：1.请切换到cad处理完cad命令; 2.确保服务已开启!") #这里简化处理，原则上连接超时或者在命令中都会走这个分支，（但是由于现在服务在一个线程，呈现互斥性，并不会返回命令中状态，只会返回超时!）
        else:
            st.session_state.step = 3
            st.rerun()

    st.divider()
    if st.button("重置全部流程，重新开始", type="secondary"):
        for k in list(st.session_state.keys()):
            del st.session_state[k]
        st.rerun()

def render_step3():
    """步骤3：表格归纳AI处理"""
    st.header("第三步：表格归纳AI处理")
    run_summary = st.button("调用汇总API生成多套统计表格", type="primary", use_container_width=True)
    if run_summary:
        with st.spinner("正在分类统计、汇总计算..."):
            summary_res = call_table_summary_api()
        if summary_res.get("code") == 0:
            st.success("多套汇总表格生成完成！进入第四步逐表确认编辑")
            st.session_state.table_edit_idx = 0
            st.session_state.edited_table_list = []
            st.session_state.edited_table_set = set()

            st.session_state.step = 4
            st.rerun()
        else:
            st.error(f"汇总接口异常：{summary_res.get('msg')}")
    st.divider()
    if st.button("重置全部流程，重新开始", type="secondary"):
        for k in list(st.session_state.keys()):
            del st.session_state[k]
        st.rerun()


#table_rows是list[dict], 每个元素是字典如下：
#    {
#         "qty": 2,
#         "func": "",
#         "name": "防水栓",
#         "supplier": "",
#         "strTableId": "1",
#         "supplier_no": "7165-0622",
#         "harness_code": "",
#         "harness_name": ""
#       }
def trans_table_rows(table_rows):
    #table_rows = [{k: v for k, v in d.items() if k != 'strTableId'} for d in table_rows]
    key_trans = {
        'qty' : '用量',
        'func' : '功能',
        'name' : '名称',
        'supplier' : '供应商',
        'supplier_no' : '供应商件号',
        'harness_code' : '线束件号',
        'harness_name' : '线束名称',
    }

    trans_keys = [k for k in key_trans.keys()]
    return [ {key_trans[k]: d[k] for k in trans_keys if k in d} for d in table_rows ]


def render_step4():
    """步骤4：多表格循环确认编辑"""
    st.header("第四步：汇总表格逐张确认编辑")

    task = "agent_tablesum_sum_taskid_001"
    table_list = get_taskid_db_tablesum(task)

    total_table_num = len(table_list)
    current_idx = st.session_state.table_edit_idx

    if total_table_num == 0:
        st.warning("暂无汇总表格数据，请返回第三步重新生成汇总表格")
    else:
        if current_idx < total_table_num:
            current_table = table_list[current_idx]
            table_name = current_table.get("strTableId", [])
            table_rows = current_table.get("listResultVec", [])

            minRegionPt = current_table.get("minRegionPt", [])
            maxRegionPt = current_table.get("maxRegionPt", [])

            call_table_zoom_api(minRegionPt, maxRegionPt)

            table_rows = trans_table_rows(table_rows)
            st.subheader(f"当前表格：（{current_idx+1}/{total_table_num}）")
            df_edit = pd.DataFrame(table_rows)
            #调整显示顺序
            col_order = ['线束件号', '线束名称', '供应商件号', '名称', '功能', '供应商', '用量']
            df_edit = df_edit[ [c for c in col_order if c in df_edit.columns] ]
            edit_df = st.data_editor(df_edit, num_rows="dynamic", use_container_width=True)
            col1, col2 = st.columns(2)
            
            with col1:
                if current_idx > 0:
                    prev_btn = st.button("⬅️ 上一张表格", type="secondary", use_container_width=True)
                    if prev_btn:

                        if current_idx not in st.session_state.edited_table_set:
                            st.session_state.edited_table_list.append({
                                "table_name": table_name,
                                "final_rows": edit_df.to_dict("records")
                            })

                            st.session_state.edited_table_set.add(current_idx)

                        st.session_state.table_edit_idx -= 1
                        
                        st.rerun()
                else:
                    st.button("⬅️ 已是第一张", disabled=True, use_container_width=True)
            
            with col2:
                confirm_btn = st.button("确认当前表格 ➡️", type="primary", use_container_width=True)
                if confirm_btn:
                    if current_idx not in st.session_state.edited_table_set: #防止重复的加入
                        st.session_state.edited_table_list.append({
                            "table_name": table_name,
                            "final_rows": edit_df.to_dict("records")
                        })
                        st.session_state.edited_table_set.add(current_idx)

                    st.session_state.table_edit_idx += 1
                    st.rerun()
        else:
            st.success("✅ 全部表格编辑确认完成，自动进入导出页面！")
            st.session_state.step = 5
            st.rerun()
    st.divider()
    if st.button("重置全部流程，重新开始", type="secondary"):
        for k in list(st.session_state.keys()):
            del st.session_state[k]
        st.rerun()

def merge_and_stat_tables(all_tables):
    """
    合并所有表格明细 + 按物料汇总求和
    :param all_tables: st.session_state.edited_table_list
    :return: df_all_detail(全量明细), df_summary(合并统计)
    """
    import pandas as pd
    all_detail_rows = []
    for table_item in all_tables:
        rows = table_item["final_rows"]
        # 给每行标记来源表名，方便溯源
        for row in rows:
            row["来源表格"] = table_item["table_name"]
        all_detail_rows.extend(rows)

    # 1. 全量原始明细DataFrame
    df_all_detail = pd.DataFrame(all_detail_rows)

    # 2. 分组统计：相同线束件号+供应商件号+名称，用量求和
    group_keys = ["线束件号", "供应商件号", "名称", "功能", "供应商", "线束名称"]
    # 过滤掉不存在的列防止报错
    valid_group_cols = [c for c in group_keys if c in df_all_detail.columns]

    if valid_group_cols:
        df_summary = df_all_detail.groupby(valid_group_cols, dropna=False, as_index=False)["用量"].sum()
        # 用量列重命名
        df_summary.rename(columns={"用量": "总用量"}, inplace=True)
    else:
        df_summary = pd.DataFrame()

    # 统一列排序（和页面编辑器保持一致）
    col_order = ['线束件号', '线束名称', '供应商件号', '名称', '功能', '供应商', '用量', '来源表格']
    summary_col_order = ['线束件号', '线束名称', '供应商件号', '名称', '功能', '供应商', '总用量']
    df_all_detail = df_all_detail[[c for c in col_order if c in df_all_detail.columns]]
    df_summary = df_summary[[c for c in summary_col_order if c in df_summary.columns]]

    return df_all_detail, df_summary


def render_step5():
    """步骤5：导出Excel"""
    st.header("第五步：导出Excel报表")
    
    #缓存，下次调用直接返回!
    @st.cache_data
    def cached_generate_excel(all_tables):
        return generate_excel(all_tables)
    
    excel_buf = cached_generate_excel(st.session_state.edited_table_list)
    
    download_excel = st.download_button(
        "下载多sheet Excel汇总表",
        data=excel_buf,
        file_name=f"CAD多表格汇总_{datetime.now().strftime('%Y%m%d_%H%M')}.xlsx",
        mime="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        use_container_width=True
    )
    if download_excel:
        st.session_state.all_finished = True
        st.rerun()
    
    if st.session_state.all_finished:
        st.success("🎉 文件导出完成，全部流程执行完毕！顶部所有步骤已标记完成")
    
    st.divider()
    if st.button("重置全部流程，重新开始", type="secondary"):
        for k in list(st.session_state.keys()):
            del st.session_state[k]
        st.rerun()

# ====================== 重置函数 ======================
def reset_all():
    """重置所有会话状态"""
    for k in list(st.session_state.keys()):
        del st.session_state[k]
    st.rerun()

# ====================== 主函数 ======================
def main():
    """主应用入口函数"""
    # 设置页面配置（必须在最前面）
    st.set_page_config(
        page_title="线束CAD表格信息汇总处理系统",
        page_icon="📊",
        layout="wide"
    )
    
    # 初始化会话状态
    init_session()
    
    # 应用CAD主题
    apply_cad_theme()
    
    # 渲染标题
    st.title("CAD图纸表格智能汇总平台")
    st.divider()
    
    # 渲染步骤指示器
    render_step_indicators()
    
    # 根据当前步骤渲染对应的界面
    st.subheader("功能操作区")
    
    if st.session_state.step == 1:
        render_step1()
    elif st.session_state.step == 2:
        render_step2()
    elif st.session_state.step == 3:
        render_step3()
    elif st.session_state.step == 4:
        render_step4()
    elif st.session_state.step == 5:
        render_step5()

# ====================== 程序入口 ======================
if __name__ == "__main__":
    main()