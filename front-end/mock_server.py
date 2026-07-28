"""CAD 后端 Mock 服务 - 模拟 3 个 API 接口，供前端联调使用
启动: python mock_server.py
端口: 127.0.0.1:18088
"""
import json
import time
from http.server import HTTPServer, BaseHTTPRequestHandler


# ====================== 假数据 ======================
BOM_DATA = {
    "items": [
        {"serial_no": "1", "drawing_no": "SD160.1", "name": "160 主动车轮",
         "quantity": "1", "material": "40Cr", "unit_weight": "3.5", "total_weight": "3.5", "remark": ""},
        {"serial_no": "2", "drawing_no": "SD160.2", "name": "160 主动轴承座(非电机侧)",
         "quantity": "1", "material": "ZG200-400", "unit_weight": "2.2", "total_weight": "2.2", "remark": ""},
        {"serial_no": "3", "drawing_no": "SD160.3", "name": "160 主动轴承座(电机侧)",
         "quantity": "1", "material": "ZG200-400", "unit_weight": "3.4", "total_weight": "3.4", "remark": ""},
        {"serial_no": "4", "drawing_no": "GB/T276-1994", "name": "深沟球轴承6213-2Z",
         "quantity": "2", "material": "成品", "unit_weight": "0.03", "total_weight": "0.06", "remark": ""},
        {"serial_no": "5", "drawing_no": "GB/T70.1-2000", "name": "内六角螺钉M10x16",
         "quantity": "8", "material": "8.5级", "unit_weight": "0.01", "total_weight": "0.08", "remark": ""},
        {"serial_no": "6", "drawing_no": "GB/T97.1-2002", "name": "平垫 10",
         "quantity": "8", "material": "A级", "unit_weight": "0.01", "total_weight": "0.08", "remark": ""},
    ]
}

TITLEBLOCK_DATA = {
    "enterprise_name": "中望重工机械有限公司",
    "drawing_name": "导轮轴套装配图",
    "drawing_no": "ZRX-2026-001",
    "product_or_material_mark": "45# 钢",
    "weight": "3.5kg",
    "designer": "张三",
    "reviewer": "李四",
    "standardizer": "王工",
    "process_engineer": "",
    "drawing_date": "2026-07-15",
    "sheet_total": "3",
    "sheet_current": "1",
    "scale": "1:2",
    "drawing_sheet_count": "3",
    "sheet_size": "A3",
    "checker": "",
    "final_reviewer": "赵六",
    "approver": "陈总",
    "drawer": "张三",
    "assembly_name": "行走机构总成",
    "assembly_drawing_no": "ZRX-2026-000",
    "unit_weight": "1.75kg",
    "position_no": "02",
    "quantity": "2",
    "revision_no": "A",
    "remark": "调质处理 HB220-250",
}

BBOX = {"min_x": 120.5, "min_y": 80.2, "max_x": 420.3, "max_y": 210.8}
HANDLES = ["1FA8", "1FA9", "1FAA", "1FAB"]


class MockHandler(BaseHTTPRequestHandler):

    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def _json(self, data: dict, status: int = 200):
        body = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self._cors()
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_GET(self):
        if self.path == "/api/status":
            self._json({
                "status": "ok",
                "cad_version": "ZWCAD 2025 (Mock)",
                "plugin": "simple_table_prase.zrx",
            })
        else:
            self._json({"error": "not found"}, 404)

    def do_POST(self):
        if self.path == "/api/select_and_process":
            # 读取请求体
            length = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(length)) if length > 0 else {}
            mode = body.get("convert_mode", 1)

            time.sleep(0.5)  # 模拟处理延迟

            if mode == 1:
                self._json({
                    "success": True,
                    "message": "BOM 表识别完成 (Mock)",
                    "convert_mode": 1,
                    "bbox": BBOX,
                    "selected_handles": HANDLES,
                    "extracted_fields": BOM_DATA,
                })
            else:
                self._json({
                    "success": True,
                    "message": "标题栏识别完成 (Mock)",
                    "convert_mode": 2,
                    "bbox": {**BBOX, "min_y": 60.0, "max_y": 150.0},
                    "selected_handles": HANDLES[:2],
                    "extracted_fields": TITLEBLOCK_DATA,
                })

        elif self.path == "/api/writeback_table":
            length = int(self.headers.get("Content-Length", 0))
            body = json.loads(self.rfile.read(length)) if length > 0 else {}

            print(f"[Mock] 收到回写请求: mode={body.get('convert_mode')}, "
                  f"style={body.get('style_type')}, handles={body.get('erase_handles')}")

            time.sleep(0.5)
            self._json({
                "success": True,
                "message": "ZcDbTable 已生成，旧实体已擦除 (Mock)",
            })

        else:
            self._json({"error": "not found"}, 404)

    def log_message(self, format, *args):
        print(f"[Mock] {args[0]}")


if __name__ == "__main__":
    server = HTTPServer(("127.0.0.1", 18088), MockHandler)
    print("Mock 服务已启动: http://127.0.0.1:18088")
    print("接口列表:")
    print("  GET  /api/status             - 健康检查")
    print("  POST /api/select_and_process - 框选提取")
    print("  POST /api/writeback_table    - 回写表格")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n服务已停止")
        server.shutdown()
