import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from xml.etree import ElementTree as ET
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[2]
WORKBOOK = Path(os.environ.get("GAME_DATA_TEST_WORKBOOK", ROOT / "GameData/Excel/Player.xlsx"))
GENERATOR = ROOT / "Tools/GameDataGenerator/bin/Release/net10.0/GameDataGenerator.dll"
SHEET = "xl/worksheets/sheet1.xml"
NS = "{http://schemas.openxmlformats.org/spreadsheetml/2006/main}"
REL_NS = "{http://schemas.openxmlformats.org/officeDocument/2006/relationships}"
PKG_NS = "{http://schemas.openxmlformats.org/package/2006/relationships}"
TYPES_NS = "{http://schemas.openxmlformats.org/package/2006/content-types}"
ROWS = [
    ["#Table", "PlayerMovement", "", "", ""],
    ["#Target", "Shared", "", "", ""],
    ["", "", "", "", ""],
    ["#Key", "Primary", "", "", ""],
    ["#Scope", "Shared", "Server", "Server", "Client"],
    ["#Type", "uint32", "uint32", "uint32", "uint32"],
    ["#Required", "true", "true", "true", "true"],
    ["#Min", "1", "", "", ""],
    ["#Max", "", "", "", ""],
    ["#Default", "", "", "", ""],
    ["#Allowed", "", "", "", ""],
    ["#Reference", "", "", "", ""],
    ["#Description", "이동 프로필 ID", "기본 이동 속도(cm/s)", "충돌 반경(cm)", "외형 스케일(1/1000)"],
    ["#Field", "PlayerMovementDataId", "MoveSpeedCmPerSecond", "CollisionRadiusCm", "VisualScalePermille"],
    ["", "1", "450", "35", "1000"],
]


def replace_cell(source, destination, address, cell_type, value, formula=None, sheet_name=None):
    # 회귀용 사본의 셀 한 개만 바꾼다. 실제 Excel 원본은 저장하지 않는다.
    with ZipFile(source) as original, ZipFile(destination, "w") as candidate:
        for item in original.infolist():
            data = original.read(item)
            if item.filename == SHEET:
                sheet = ET.fromstring(data)
                cell = sheet.find(f".//{NS}c[@r='{address}']")
                if cell is None:
                    row_number = address.lstrip("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
                    row = sheet.find(f"{NS}sheetData/{NS}row[@r='{row_number}']")
                    assert row is not None, address
                    cell = ET.SubElement(row, NS + "c", {"r": address})
                    row[:] = sorted(row, key=lambda c: c.get("r").rstrip("0123456789"))
                for child in list(cell):
                    cell.remove(child)
                cell.set("t", cell_type)
                if formula is not None:
                    ET.SubElement(cell, NS + "f").text = formula
                if cell_type == "inlineStr":
                    inline = ET.SubElement(cell, NS + "is")
                    text = ET.SubElement(inline, NS + "t", {"{http://www.w3.org/XML/1998/namespace}space": "preserve"})
                    text.text = value
                else:
                    ET.SubElement(cell, NS + "v").text = value
                data = ET.tostring(sheet, encoding="utf-8", xml_declaration=True)
            elif sheet_name is not None and item.filename == "xl/workbook.xml":
                workbook = ET.fromstring(data)
                workbook.find(f"{NS}sheets/{NS}sheet").set("name", sheet_name)
                data = ET.tostring(workbook, encoding="utf-8", xml_declaration=True)
            candidate.writestr(item, data)


class ExcelToCsvTests(unittest.TestCase):
    def setUp(self):
        with self.subTest("빌드된 CLI"):
            self.assertTrue(GENERATOR.is_file(), "GameDataGenerator Release CLI가 아직 빌드되지 않았습니다.")
        temp = tempfile.TemporaryDirectory(prefix="game-data-csv-", dir=os.environ.get("TMPDIR"))
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.input = self.root / "Excel"
        self.input.mkdir()
        self.output = self.root / "Csv"
        self.xlsx = self.input / WORKBOOK.name
        self.xlsx.write_bytes(WORKBOOK.read_bytes())
        self.csv = self.output / "PlayerMovement.csv"

    def run_generator(self, success=True):
        result = subprocess.run(
            ["dotnet", str(GENERATOR), str(self.input), str(self.output)],
            cwd=self.root,
            capture_output=True,
            check=False,
            timeout=20,
        )
        stdout = result.stdout.decode("utf-8", errors="replace")
        stderr = result.stderr.decode("utf-8", errors="replace")
        self.assertEqual(result.returncode, 0 if success else 1, (stdout, stderr))
        return stderr

    def read_rows(self):
        with self.csv.open(encoding="utf-8", newline="") as file:
            return list(csv.reader(file))

    def test_player_movement_preserves_metadata_and_empty_cells(self):
        self.run_generator()
        self.assertEqual(self.read_rows(), ROWS)
        expected = "".join(",".join(row) + "\r\n" for row in ROWS).encode("utf-8")
        self.assertEqual(self.csv.read_bytes(), expected)
        self.run_generator()
        self.assertEqual(self.csv.read_bytes(), expected)
        self.assertEqual(self.xlsx.read_bytes(), WORKBOOK.read_bytes())
        self.assertEqual(list(self.output.iterdir()), [self.csv])

    def test_csv_quotes_comma_double_quote_and_embedded_newline(self):
        value = '앞, "중간"\n끝  '
        replace_cell(WORKBOOK, self.xlsx, "E13", "inlineStr", value)
        self.run_generator()
        expected = [row[:] for row in ROWS]
        expected[12][-1] = value
        self.assertEqual(self.read_rows(), expected)
        self.assertIn('"앞, ""중간""\n끝  "'.encode("utf-8"), self.csv.read_bytes())

    def test_one_workbook_generates_one_csv_per_sheet(self):
        # 운영 시트는 하나만 유지하고 두 번째 시트는 테스트 사본에서만 만든다.
        with ZipFile(WORKBOOK) as original, ZipFile(self.xlsx, "w") as candidate:
            sheet = ET.fromstring(original.read(SHEET))
            for tag in ("tableParts", "legacyDrawing", "pageSetup"):
                element = sheet.find(NS + tag)
                if element is not None:
                    sheet.remove(element)
            name = sheet.find(f".//{NS}c[@r='B1']")
            name.set("t", "inlineStr")
            name.remove(name.find(NS + "v"))
            ET.SubElement(ET.SubElement(name, NS + "is"), NS + "t").text = "OtherProfile"
            candidate.writestr("xl/worksheets/sheet2.xml", ET.tostring(sheet, encoding="utf-8"))
            for item in original.infolist():
                data = original.read(item)
                if item.filename == "xl/workbook.xml":
                    root = ET.fromstring(data)
                    ET.SubElement(root.find(NS + "sheets"), NS + "sheet", {
                        "name": "OtherProfile", "sheetId": "2", REL_NS + "id": "rId99",
                    })
                    data = ET.tostring(root, encoding="utf-8")
                elif item.filename == "xl/_rels/workbook.xml.rels":
                    ET.register_namespace("", PKG_NS[1:-1])
                    root = ET.fromstring(data)
                    ET.SubElement(root, PKG_NS + "Relationship", {
                        "Id": "rId99", "Target": "worksheets/sheet2.xml",
                        "Type": REL_NS[1:-1] + "/worksheet",
                    })
                    data = ET.tostring(root, encoding="utf-8")
                elif item.filename == "[Content_Types].xml":
                    ET.register_namespace("", TYPES_NS[1:-1])
                    root = ET.fromstring(data)
                    ET.SubElement(root, TYPES_NS + "Override", {
                        "PartName": "/xl/worksheets/sheet2.xml",
                        "ContentType": "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml",
                    })
                    data = ET.tostring(root, encoding="utf-8")
                candidate.writestr(item, data)

        self.run_generator()
        self.assertEqual(sorted(p.name for p in self.output.iterdir()), ["OtherProfile.csv", "PlayerMovement.csv"])
        self.assertEqual(self.read_rows(), ROWS)
        with (self.output / "OtherProfile.csv").open(encoding="utf-8", newline="") as file:
            expected = [row[:] for row in ROWS]
            expected[0][1] = "OtherProfile"
            self.assertEqual(list(csv.reader(file)), expected)

    def test_formula_and_error_cells_leave_existing_csv_untouched(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        for cell_type, value, formula, reason in [("n", "450", "200+250", "formula"), ("e", "#DIV/0!", None, "error")]:
            with self.subTest(reason=reason):
                replace_cell(WORKBOOK, self.xlsx, "C15", cell_type, value, formula)
                message = self.run_generator(success=False)
                self.assertIn("Player.xlsx:PlayerMovement!C15", message)
                self.assertIn(reason, message.lower())
                self.assertEqual(self.csv.read_bytes(), prior)
                self.assertEqual(list(self.output.iterdir()), [self.csv])

    def test_bad_metadata_and_filename_collision_publish_nothing(self):
        for address, value, name in [("A11", "#Allow", "second.xlsx"), ("A14", "#Fields", "second.xlsx"),
                                     ("B1", "NotTheSheet", "second.xlsx"), ("B1", "CON", "second.xlsx")]:
            with self.subTest(address=address, value=value):
                candidate = self.input / name
                replace_cell(WORKBOOK, candidate, address, "inlineStr", value)
                try:
                    message = self.run_generator(success=False)
                    self.assertIn(f"{candidate.name}:PlayerMovement!{address}", message)
                    self.assertFalse(self.output.exists())
                finally:
                    candidate.unlink()

        self.run_generator()
        prior = self.csv.read_bytes()
        collision = self.input / "z-second.xlsx"
        replace_cell(WORKBOOK, collision, "B1", "inlineStr", "playermovement", sheet_name="playermovement")
        message = self.run_generator(success=False)
        self.assertIn("z-second.xlsx:playermovement!B1", message)
        self.assertIn("duplicate", message.lower())
        self.assertEqual(self.csv.read_bytes(), prior)
        self.assertEqual(list(self.output.iterdir()), [self.csv])

    def test_invalid_declarations_preserve_previous_output(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        cases = [
            ("C4", "Index", "#Key"), ("C4", "Primary", "#Key"),
            ("C5", "Both", "#Scope"), ("C6", "string", "#Type"),
            ("B6", "int32", "#Key"), ("C7", "maybe", "#Required"), ("C7", "", "#Required"),
            ("A11", "#Allow", "#Allowed"), ("B8", "0", "#Min must be at least 1"),
            ("B9", "0", "#Max must be at least 1"),
            ("C8", "-1", "#Min"),
            ("B10", "1", "#Default"), ("C11", "10||20", "#Allowed"),
            ("C11", "10|10", "#Allowed"), ("C11", "4294967296", "#Allowed"),
            ("C12", "PlayerMovement.PlayerMovementDataId", "#Reference"),
        ]
        for address, value, label in cases:
            with self.subTest(address=address, value=value):
                replace_cell(WORKBOOK, self.xlsx, address, "inlineStr", value)
                message = self.run_generator(success=False)
                self.assertIn(f"Player.xlsx:PlayerMovement!{address}", message)
                self.assertIn(label, message)
                self.assertEqual(self.csv.read_bytes(), prior)
                self.assertEqual(list(self.output.iterdir()), [self.csv])

    def test_valid_declarations_and_blank_description_remain_in_csv(self):
        # 실제 행 값의 허용 여부와 참조 대상 조회는 후속 단계다.
        first = self.root / "first.xlsx"
        second = self.root / "second.xlsx"
        replace_cell(WORKBOOK, first, "C11", "inlineStr", "10|20|450")
        replace_cell(first, second, "C12", "inlineStr", "Player.PlayerMovement.PlayerMovementDataId")
        replace_cell(second, self.xlsx, "E13", "inlineStr", "")
        self.run_generator()
        expected = [row[:] for row in ROWS]
        expected[10][2] = "10|20|450"
        expected[11][2] = "Player.PlayerMovement.PlayerMovementDataId"
        expected[12][4] = ""
        self.assertEqual(self.read_rows(), expected)

    def test_unreadable_workbook_names_the_file_and_preserves_existing_csv(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        (self.input / "z-corrupt.xlsx").write_bytes(b"not an xlsx package")
        message = self.run_generator(success=False)
        self.assertIn("z-corrupt.xlsx", message)
        self.assertEqual(self.csv.read_bytes(), prior)


if __name__ == "__main__":
    unittest.main()
