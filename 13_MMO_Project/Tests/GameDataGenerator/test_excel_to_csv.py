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
    ["#Workbook", "Player", "", "", ""],
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


def replace_cell(source, destination, address, cell_type, value, formula=None, sheet_name=None, sheet_path=SHEET):
    # 회귀용 사본의 셀 한 개만 바꾼다. 실제 Excel 원본은 저장하지 않는다.
    with ZipFile(source) as original, ZipFile(destination, "w") as candidate:
        for item in original.infolist():
            data = original.read(item)
            if item.filename == sheet_path:
                sheet = ET.fromstring(data)
                cell = sheet.find(f".//{NS}c[@r='{address}']")
                if cell is None:
                    row_number = address.lstrip("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
                    sheet_data = sheet.find(NS + "sheetData")
                    row = sheet_data.find(f"{NS}row[@r='{row_number}']")
                    if row is None:
                        row = ET.SubElement(sheet_data, NS + "row", {"r": row_number})
                        sheet_data[:] = sorted(sheet_data, key=lambda element: int(element.get("r")))
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
                sheet = workbook.find(f"{NS}sheets/{NS}sheet")
                original_name = sheet.get("name")
                sheet.set("name", sheet_name)
                for name in workbook.findall(f"{NS}definedNames/{NS}definedName"):
                    name.text = name.text.replace(f"'{original_name}'!", f"'{sheet_name}'!")
                    name.text = name.text.replace(f"{original_name}!", f"{sheet_name}!")
                data = ET.tostring(workbook, encoding="utf-8", xml_declaration=True)
            candidate.writestr(item, data)


def write_enum_workbook(path, sheets):
    # 운영 Enum을 만들지 않고 작은 실제 XLSX 패키지로 생성기를 검증한다.
    types = ET.Element("Types", {"xmlns": TYPES_NS[1:-1]})
    root_rels = ET.Element("Relationships", {"xmlns": PKG_NS[1:-1]})
    book_rels = ET.Element("Relationships", {"xmlns": PKG_NS[1:-1]})
    book = ET.Element(NS + "workbook")
    book_sheets = ET.SubElement(book, NS + "sheets")
    content = "application/vnd.openxmlformats-officedocument.spreadsheetml."
    ET.SubElement(types, "Default", {"Extension": "rels",
                  "ContentType": "application/vnd.openxmlformats-package.relationships+xml"})
    ET.SubElement(types, "Override", {"PartName": "/xl/workbook.xml", "ContentType": content + "sheet.main+xml"})
    ET.SubElement(root_rels, "Relationship", {"Id": "rId1", "Target": "xl/workbook.xml",
                  "Type": REL_NS[1:-1] + "/officeDocument"})
    with ZipFile(path, "w") as workbook:
        for index, (title, rows) in enumerate(sheets, 1):
            part = f"worksheets/sheet{index}.xml"
            rel_id = f"rId{index}"
            ET.SubElement(book_sheets, NS + "sheet", {"name": title, "sheetId": str(index), REL_NS + "id": rel_id})
            ET.SubElement(book_rels, "Relationship", {"Id": rel_id, "Target": part,
                          "Type": REL_NS[1:-1] + "/worksheet"})
            ET.SubElement(types, "Override", {"PartName": "/xl/" + part, "ContentType": content + "worksheet+xml"})
            sheet = ET.Element(NS + "worksheet")
            data = ET.SubElement(sheet, NS + "sheetData")
            for row_number, values in enumerate(rows, 1):
                row = ET.SubElement(data, NS + "row", {"r": str(row_number)})
                for column, value in zip("ABC", values):
                    if value is None:
                        continue
                    cell = ET.SubElement(row, NS + "c", {"r": f"{column}{row_number}"})
                    if isinstance(value, str):
                        cell.set("t", "inlineStr")
                        text = ET.SubElement(ET.SubElement(cell, NS + "is"), NS + "t")
                        text.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
                        text.text = value
                    else:
                        if isinstance(value, tuple):
                            ET.SubElement(cell, NS + "f").text = value[0]
                            value = value[1]
                        ET.SubElement(cell, NS + "v").text = str(value)
            workbook.writestr("xl/" + part, ET.tostring(sheet, encoding="utf-8"))
        for name, xml in [("[Content_Types].xml", types), ("_rels/.rels", root_rels),
                          ("xl/_rels/workbook.xml.rels", book_rels), ("xl/workbook.xml", book)]:
            workbook.writestr(name, ET.tostring(xml, encoding="utf-8"))


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

    def test_workbook_name_must_be_usable_as_folder_and_cpp_namespace(self):
        self.run_generator()
        prior = self.read_rows()
        for name in ('Bad-Name.xlsx', 'MAX_PATH.xlsx'):
            with self.subTest(name=name):
                candidate = self.input / name
                self.xlsx.rename(candidate)
                try:
                    error = self.run_generator(success=False)
                    self.assertIn(name, error)
                    self.assertIn('workbook name', error)
                    self.assertEqual(self.read_rows(), prior)
                finally:
                    candidate.rename(self.xlsx)

    def edit_cells(self, edits, destination=None, sheet_name=None):
        source = WORKBOOK
        for index, (address, cell_type, value) in enumerate(edits):
            candidate = self.root / f"edit-{index}.xlsx"
            replace_cell(source, candidate, address, cell_type, value, sheet_name=sheet_name)
            source = candidate
        (destination if destination is not None else self.xlsx).write_bytes(source.read_bytes())

    def assert_value_failure(self, address, reason, prior):
        message = self.run_generator(success=False)
        self.assertIn(f"{WORKBOOK.name}:PlayerMovement!{address}", message)
        self.assertIn(reason, message)
        self.assertEqual(self.csv.read_bytes(), prior)
        self.assertEqual(list(self.output.iterdir()), [self.csv])
        return message

    def test_enum_workbook_and_field_generate_catalog_and_symbolic_csv(self):
        enum_path = self.input / "Enum.xlsx"
        damage = [("Fire", 9), ("None", 0), (None, None), ("Ice", 2)]
        weather = [("Fire", 1), ("Rain", 4294967295)]
        header = [("Name", "Value")]
        write_enum_workbook(enum_path, [("WeatherType", header + weather), ("DamageType", header + damage)])
        enum_bytes = enum_path.read_bytes()
        self.edit_cells([("C6", "inlineStr", "enum:DamageType"), ("C15", "inlineStr", "Fire"),
                         ("D6", "inlineStr", "enum:WeatherType"), ("D7", "inlineStr", "false"),
                         ("D10", "inlineStr", "Rain"), ("D11", "inlineStr", "Fire | Rain"),
                         ("D15", "inlineStr", "")])
        self.run_generator()
        self.assertEqual(self.read_rows()[14], ["", "1", "Fire", "", "1000"])
        self.assertEqual(self.read_rows()[9][3], "Rain")
        self.assertEqual(self.read_rows()[10][3], "Fire | Rain")
        self.assertEqual(enum_path.read_bytes(), enum_bytes)
        enum_csv = self.output / "Enum.csv"
        expected = (b"Type,Name,Value\r\nDamageType,None,0\r\nDamageType,Ice,2\r\nDamageType,Fire,9\r\n"
                    b"WeatherType,Fire,1\r\nWeatherType,Rain,4294967295\r\n")
        self.assertEqual(enum_csv.read_bytes(), expected)
        self.assertEqual({path.name for path in self.output.iterdir()}, {"Enum.csv", "PlayerMovement.csv"})
        write_enum_workbook(enum_path, [("DamageType", header + damage[::-1]), ("WeatherType", header + weather[::-1])])
        self.run_generator()
        self.assertEqual(enum_csv.read_bytes(), expected)

    def test_enum_catalog_can_be_generated_without_regular_tables(self):
        self.xlsx.unlink()
        write_enum_workbook(self.input / "Enum.xlsx", [("DamageType", [("Name", "Value"), ("Fire", 9)])])
        self.run_generator()
        self.assertEqual({path.name: path.read_bytes() for path in self.output.iterdir()},
                         {"Enum.csv": b"Type,Name,Value\r\nDamageType,Fire,9\r\n"})

    def test_enum_field_rejects_missing_catalog(self):
        self.edit_cells([("C6", "inlineStr", "enum:DamageType"), ("C15", "inlineStr", "Fire")])
        error = self.run_generator(success=False)
        self.assertIn("Player.xlsx:PlayerMovement!C6", error)
        self.assertIn("Unknown enum type 'DamageType'", error)
        self.assertFalse(self.output.exists())

    def test_invalid_enum_field_preserves_catalog_and_table_csv(self):
        enum_path = self.input / "Enum.xlsx"
        write_enum_workbook(enum_path, [("DamageType", [("Name", "Value"), ("Fire", 9), ("Ice", 2)]),
                                        ("WeatherType", [("Name", "Value"), ("Rain", 9)])])
        self.run_generator()
        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.assertEqual(set(prior), {"Enum.csv", "PlayerMovement.csv"})
        # 이 정의가 다시 발행되면 이전 Enum.csv와 다른 바이트가 된다.
        write_enum_workbook(enum_path, [("DamageType", [("Name", "Value"), ("Fire", 9), ("Ice", 2)]),
                                        ("WeatherType", [("Name", "Value"), ("Rain", 8)])])
        cases = [
            ([("C6", "inlineStr", "enum:damageType")], "C6", "Unknown enum type"),
            ([("B6", "inlineStr", "enum:DamageType")], "B6", "#Key=Primary"),
            ([("C15", "inlineStr", "fire")], "C15", "Unknown enum member"),
            ([("C15", "inlineStr", "Rain")], "C15", "Unknown enum member"),
            ([("C15", "n", "9")], "C15", "requires a text Excel cell"),
            ([("C15", "inlineStr", "9")], "C15", "Unknown enum member"),
            ([("C15", "inlineStr", "")], "C15", "#Required"),
            ([("C7", "inlineStr", "false"), ("C10", "inlineStr", "Rain")], "C10", "Unknown enum member"),
            ([("C7", "inlineStr", "false"), ("C10", "n", "9")], "C10", "requires a text Excel cell"),
            ([("C11", "inlineStr", "Fire | Rain")], "C11", "Unknown enum member"),
            ([("C11", "inlineStr", "Fire | Fire")], "C11", "duplicate"),
            ([("C11", "inlineStr", "Ice")], "C15", "#Allowed"),
            ([("C8", "n", "1")], "C8", "#Min"),
            ([("C12", "inlineStr", "Player.PlayerMovement.PlayerMovementDataId")], "C12", "type must match uint32"),
        ]
        for edits, cell, reason in cases:
            with self.subTest(edits=edits):
                self.edit_cells([("C6", "inlineStr", "enum:DamageType"), ("C15", "inlineStr", "Fire")] + edits)
                error = self.run_generator(success=False)
                self.assertIn(f"Player.xlsx:PlayerMovement!{cell}", error)
                self.assertIn(reason, error)
                self.assertEqual({path.name: path.read_bytes() for path in self.output.iterdir()}, prior)

        self.edit_cells([("C6", "inlineStr", "enum:DamageType"), ("C15", "inlineStr", "Fire")])
        self.run_generator()
        self.assertEqual((self.output / "Enum.csv").read_bytes(),
                         prior["Enum.csv"].replace(b"WeatherType,Rain,9", b"WeatherType,Rain,8"))
        self.assertEqual(self.read_rows()[14][2], "Fire")

    def test_invalid_enum_definition_preserves_existing_csvs(self):
        enum_path = self.input / "Enum.xlsx"
        write_enum_workbook(enum_path, [("DamageType", [("Name", "Value"), ("Fire", 1), ("Ice", 2)])])
        self.run_generator()
        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.assertEqual(set(prior), {"Enum.csv", "PlayerMovement.csv"})

        self.edit_cells([("C15", "n", "451")])
        write_enum_workbook(enum_path, [("DamageType", [("Name", "Value"), ("Fire", 9), ("Ice", 9)])])
        error = self.run_generator(success=False)
        self.assertIn("Enum.xlsx:DamageType!B3", error)
        self.assertIn("Duplicate enum value 9", error)
        self.assertEqual({path.name: path.read_bytes() for path in self.output.iterdir()}, prior)

    def test_invalid_enum_definitions_fail_before_output(self):
        enum_path = self.input / "Enum.xlsx"
        cases = [
            ("class", [("Name", "Value"), ("Fire", 1)], "A1", "type name"),
            ("DamageType", [("Member", "Value"), ("Fire", 1)], "A1", "Name"),
            ("DamageType", [("Name", "Value")], "A2", "at least one"),
            ("DamageType", [("Name", "Value"), ("Fire", 1), ("Fire", 2)], "A3", "Duplicate enum member"),
            ("DamageType", [("Name", "Value"), ("Fire", 1), ("Ice", 1)], "B3", "Duplicate enum value"),
            ("DamageType", [("Name", "Value"), ("return", 1)], "A2", "member name"),
            ("DamageType", [("Name", "Value"), ("Ice__Wall", 1)], "A2", "member name"),
            ("DamageType", [("Name", "Value"), ("Fire", None)], "B2", "uint32"),
            ("DamageType", [("Name", "Value"), ("Fire", "1")], "B2", "uint32"),
            ("DamageType", [("Name", "Value"), ("Fire", -1)], "B2", "uint32"),
            ("DamageType", [("Name", "Value"), ("Fire", 1.5)], "B2", "uint32"),
            ("DamageType", [("Name", "Value"), ("Fire", 4294967296)], "B2", "uint32"),
            ("DamageType", [("Name", "Value"), ("Fire", ("1+1", 2))], "B2", "Formula"),
            ("DamageType", [("Name", "Value"), ("Fire", 1, "extra")], "C2", "columns A and B"),
        ]
        for title, rows, cell, reason in cases:
            with self.subTest(title=title, rows=rows):
                write_enum_workbook(enum_path, [(title, rows)])
                error = self.run_generator(success=False)
                self.assertIn(f"Enum.xlsx:{title}!{cell}", error)
                self.assertIn(reason, error)
                self.assertFalse(self.output.exists())

    def test_enum_csv_name_is_reserved_even_without_enum_workbook(self):
        self.edit_cells([("B1", "inlineStr", "eNuM")], sheet_name="eNuM")
        error = self.run_generator(success=False)
        self.assertIn("Player.xlsx:eNuM!B1", error)
        self.assertIn("Enum.csv is reserved", error)
        self.assertFalse(self.output.exists())

    def test_integer_arrays_preserve_order_duplicates_and_full_range(self):
        self.edit_cells([
            ("C6", "inlineStr", "int32[]"), ("C15", "inlineStr", " [-2147483648 | +2 | 2147483647 | 2] "),
            ("D6", "inlineStr", "uint32[]"), ("D15", "inlineStr", "[4294967295|0|1]"),
        ])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual(rows[5][2:4], ["int32[]", "uint32[]"])
        self.assertEqual(rows[14], ["", "1", "[-2147483648 | 2 | 2147483647 | 2]", "[4294967295 | 0 | 1]", "1000"])

    def test_array_blanks_and_explicit_empty_values_stay_distinct(self):
        self.edit_cells([
            ("C6", "inlineStr", "uint32[]"), ("C7", "inlineStr", "false"),
            ("C10", "inlineStr", "[ 002|1 ]"), ("C15", "inlineStr", ""),
            ("D6", "inlineStr", "uint32[]"), ("D7", "inlineStr", "false"),
            ("D10", "inlineStr", "[1]"), ("D15", "inlineStr", "[ ]"),
            ("E6", "inlineStr", "uint32[]"), ("E15", "inlineStr", "[]"),
        ])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual(rows[9][2:], ["[2 | 1]", "[1]", ""])
        self.assertEqual(rows[14][2:], ["", "[]", "[]"])

    def test_string_arrays_round_trip_quoted_delimiters_and_whitespace(self):
        first = '" 검 | 방패, [각인] "'
        second = '"활 ""특수""\n2행\u2028"'
        allowed = f'{first} | {second}'
        self.edit_cells([
            ("C6", "inlineStr", "string[]"), ("C7", "inlineStr", "false"),
            ("C10", "inlineStr", f'[{first}]'), ("C11", "inlineStr", allowed),
            ("C15", "inlineStr", f'[{second}|{first}]'),
        ])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual(rows[9][2], f'[{first}]')
        self.assertEqual(rows[10][2], allowed)
        self.assertEqual(rows[14][2], f'[{second} | {first}]')
        self.assertEqual(len(rows), 15)

    def test_float_and_enum_arrays_validate_each_element(self):
        write_enum_workbook(self.input / "Enum.xlsx", [("DamageType", [("Name", "Value"), ("Fire", 9), ("Ice", 2)])])
        edits = [
            ("C6", "inlineStr", "float32[]"), ("C8", "n", "1"), ("C9", "n", "2"),
            ("C11", "inlineStr", "1.0000000596046448 | 2"),
            ("D6", "inlineStr", "enum:DamageType[]"), ("D11", "inlineStr", "Fire | Ice"),
            ("C15", "inlineStr", "[2e0 | 1.0000000596046448]"), ("D15", "inlineStr", "[Ice|Fire]"),
        ]
        self.edit_cells(edits)
        self.run_generator()
        self.assertEqual(self.read_rows()[14][2:4], ["[2 | 1.0000001]", "[Ice | Fire]"])
        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.assertEqual(set(prior), {"Enum.csv", "PlayerMovement.csv"})
        write_enum_workbook(self.input / "Enum.xlsx", [("DamageType", [("Name", "Value"), ("Fire", 8), ("Ice", 2)])])
        for cell, changes, reason in [
            ("C15", [("C15", "[2 | 1]")], "Array element 2: #Allowed"),
            ("C15", [("C15", "[2 | 0.5]")], "Array element 2: #Min"),
            ("C15", [("C15", "[2 | 3]")], "Array element 2: #Max"),
            ("C15", [("C15", "[2 | 1e-80]")], "Array element 2 must be a valid float32"),
            ("D15", [("D15", "[Ice | fire]")], "Array element 2: Unknown enum member"),
            ("D15", [("D15", "[Ice | 8]")], "Array element 2: Unknown enum member"),
            ("D15", [("D11", "Ice")], "Array element 2: #Allowed"),
        ]:
            with self.subTest(cell=cell, changes=changes):
                self.edit_cells(edits + [(address, "inlineStr", value) for address, value in changes])
                error = self.run_generator(success=False)
                self.assertIn(f"Player.xlsx:PlayerMovement!{cell}", error)
                self.assertIn(reason, error)
                self.assertEqual({path.name: path.read_bytes() for path in self.output.iterdir()}, prior)
        self.edit_cells(edits)
        self.run_generator()
        self.assertIn(b"DamageType,Fire,8", (self.output / "Enum.csv").read_bytes())

    def test_invalid_array_syntax_and_values_preserve_existing_output(self):
        self.edit_cells([("C6", "inlineStr", "uint32[]"), ("C15", "inlineStr", "[0 | 2]")])
        self.run_generator()
        prior = self.csv.read_bytes()
        for field_type, kind, value, reason in [
            ("uint32[]", "n", "1", "requires a text Excel cell"),
            ("uint32[]", "inlineStr", "", "#Required"),
            ("uint32[]", "inlineStr", "1 | 2", "Invalid array"),
            ("uint32[]", "inlineStr", "[1 | [2]]", "Array element 2"),
            ("uint32[]", "inlineStr", "[1 | ]", "Array element 2"),
            ("uint32[]", "inlineStr", "[1 || 2]", "Array element 2"),
            ("uint32[]", "inlineStr", '[1 | "2"]', "Array element 2"),
            ("uint32[]", "inlineStr", "[4294967296]", "Array element 1"),
            ("int32[]", "inlineStr", "[-2147483649]", "Array element 1"),
            ("float32[]", "inlineStr", "[1e40]", "Array element 1"),
            ("string[]", "inlineStr", '["a" | b]', "Invalid array"),
            ("string[]", "inlineStr", '["a" | "b]', "Invalid array"),
            ("string[]", "inlineStr", '["a" | ]', "Invalid array"),
            ("string[]", "inlineStr", '["a" x]', "Invalid array"),
            ("string[]", "inlineStr", '[""]', "Array element 1"),
        ]:
            with self.subTest(type=field_type, value=value):
                self.edit_cells([("C6", "inlineStr", field_type), ("C15", kind, value)])
                self.assert_value_failure("C15", reason, prior)

    def test_invalid_array_declarations_and_defaults(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        valid = [("C6", "inlineStr", "uint32[]"), ("C7", "inlineStr", "false"), ("C15", "inlineStr", "[2]")]
        for edits, cell, reason in [
            ([("C6", "inlineStr", "bool[]")], "C6", "type"),
            ([("C6", "inlineStr", "int32[][]")], "C6", "type"),
            ([("C6", "inlineStr", "enum:Unknown[]")], "C6", "Unknown enum type 'Unknown'"),
            ([("B6", "inlineStr", "uint32[]")], "B6", "#Key=Primary"),
            ([("C12", "inlineStr", "Player.PlayerMovement.PlayerMovementDataId")], "C12", "#Reference"),
            ([("C6", "inlineStr", "string[]"), ("C8", "n", "1")], "C8", "#Min"),
            ([("C8", "n", "3"), ("C9", "n", "2")], "C9", "cannot be less than #Min"),
            ([("C11", "inlineStr", "2 | 02")], "C11", "duplicate"),
            ([("C6", "inlineStr", "string[]"), ("C11", "inlineStr", '"a" | b')], "C11", "quoted"),
            ([("C6", "inlineStr", "string[]"), ("C11", "inlineStr", '"a" | "a"')], "C11", "duplicate"),
            ([("C10", "n", "1")], "C10", "requires a text Excel cell"),
            ([("C10", "inlineStr", "[2 | 3]"), ("C11", "inlineStr", "2")], "C10", "Array element 2: #Allowed"),
            ([("C10", "inlineStr", "[2 | 3]"), ("C9", "n", "2")], "C10", "Array element 2: #Max"),
            ([("C7", "inlineStr", "true"), ("C10", "inlineStr", "[]")], "C10", "optional field"),
        ]:
            with self.subTest(edits=edits):
                self.edit_cells(valid + edits)
                self.assert_value_failure(cell, reason, prior)

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

    def write_two_sheet_workbook(self):
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

    def test_one_workbook_generates_one_csv_per_sheet(self):
        self.write_two_sheet_workbook()
        self.run_generator()
        self.assertEqual(sorted(p.name for p in self.output.iterdir()), ["OtherProfile.csv", "PlayerMovement.csv"])
        self.assertEqual(self.read_rows(), ROWS)
        with (self.output / "OtherProfile.csv").open(encoding="utf-8", newline="") as file:
            expected = [row[:] for row in ROWS]
            expected[0][1] = "OtherProfile"
            self.assertEqual(list(csv.reader(file)), expected)

    @unittest.skipUnless(os.name == "nt", "Windows file sharing required")
    def test_locked_later_csv_rolls_back_the_first_sheet(self):
        self.write_two_sheet_workbook()
        self.run_generator()
        other = self.output / "OtherProfile.csv"
        before = {p.name: p.read_bytes() for p in self.output.iterdir()}
        candidate = self.root / "candidate.xlsx"
        replace_cell(self.xlsx, candidate, "C15", "n", "451")
        replace_cell(candidate, self.xlsx, "C15", "n", "452", sheet_path="xl/worksheets/sheet2.xml")

        with other.open("rb"):
            message = self.run_generator(success=False)
        self.assertEqual(self.csv.read_bytes(), before[self.csv.name], "Earlier CSV was not rolled back")
        self.assertIn(other.name, message)
        self.assertEqual({p.name: p.read_bytes() for p in self.output.iterdir()}, before)

        self.run_generator()
        self.assertNotEqual(self.csv.read_bytes(), before[self.csv.name])
        self.assertNotEqual(other.read_bytes(), before[other.name])

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
        collision = self.input / "z_second.xlsx"
        replace_cell(WORKBOOK, collision, "B1", "inlineStr", "playermovement", sheet_name="playermovement")
        message = self.run_generator(success=False)
        self.assertIn("z_second.xlsx:playermovement!B1", message)
        self.assertIn("duplicate", message.lower())
        self.assertEqual(self.csv.read_bytes(), prior)
        self.assertEqual(list(self.output.iterdir()), [self.csv])

    def test_invalid_declarations_preserve_previous_output(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        cases = [
            ("C4", "Index", "#Key"), ("C4", "Primary", "#Key"),
            ("C5", "Both", "#Scope"), ("C6", "bool", "#Type"),
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
        first = self.root / "first.xlsx"
        replace_cell(WORKBOOK, first, "C11", "inlineStr", "10|20|450")
        replace_cell(first, self.xlsx, "E13", "inlineStr", "")
        self.run_generator()
        expected = [row[:] for row in ROWS]
        expected[10][2] = "10|20|450"
        expected[12][4] = ""
        self.assertEqual(self.read_rows(), expected)

    def test_float32_and_string_round_trip_to_csv(self):
        text = '  검, "방패" | 활\n001\u2028둘\u2029셋\u0085끝  '
        self.edit_cells([("C6", "inlineStr", "float32"), ("C8", "n", "0.1"),
                         ("C9", "inlineStr", "0.3"), ("C11", "inlineStr", "0.1 | 0.3"),
                         ("C15", "n", "0.30000000000000004"),
                         ("D6", "inlineStr", "string"), ("D15", "inlineStr", text),
                         ("E6", "inlineStr", "string"), ("E15", "inlineStr", "001")])
        self.run_generator()
        self.assertEqual(self.read_rows()[14], ["", "1", "0.3", text, "001"])

    def test_float32_decimal_text_rounds_directly_but_number_cells_round_the_excel_value(self):
        midpoint = "1.0000000596046448"
        scalar = [("C6", "inlineStr", "float32")]
        self.edit_cells(scalar + [("C11", "n", midpoint), ("C15", "n", midpoint)])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual((rows[10][2], rows[14][2]), ("1", "1"))
        prior = self.csv.read_bytes()
        self.edit_cells(scalar + [("C11", "inlineStr", midpoint), ("C15", "n", "1")])
        self.assert_value_failure("C15", "#Allowed", prior)
        self.edit_cells(scalar + [("C11", "inlineStr", midpoint), ("C15", "n", "1.0000001192092896")])
        self.run_generator()
        self.assertEqual(self.read_rows()[14][2], "1.0000001")

    def test_float32_rejects_wrong_cell_kinds_and_unrepresentable_values(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        for cell_type, value, reason in [("inlineStr", "1.25", "numeric Excel cell"), ("b", "1", "numeric Excel cell"),
                                          ("n", "3.5e38", "float32"), ("n", "1e-50", "float32")]:
            with self.subTest(cell_type=cell_type, value=value):
                self.edit_cells([("C6", "inlineStr", "float32"), ("C15", cell_type, value)])
                self.assert_value_failure("C15", reason, prior)

    def test_float32_limits_default_and_allowed_use_float32_values(self):
        scalar = [("C6", "inlineStr", "float32"), ("C7", "inlineStr", "false")]
        self.edit_cells(scalar + [("C10", "inlineStr", "0.100000001"), ("C11", "inlineStr", "0.1"),
                                  ("C15", "inlineStr", ""), ("D6", "inlineStr", "float32"),
                                  ("D15", "n", "3.4028235e38"), ("E6", "inlineStr", "float32"), ("E15", "n", "1e-45")])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual(rows[9][2], "0.100000001")
        self.assertEqual(rows[14][2:], ["", "3.4028235E+38", "1E-45"])
        prior = self.csv.read_bytes()
        cases = [
            ([("C8", "n", "2.5"), ("C15", "n", "2.4")], "C15", "#Min"),
            ([("C9", "n", "2.5"), ("C15", "n", "2.6")], "C15", "#Max"),
            ([("C11", "inlineStr", "2.5|3.5"), ("C15", "n", "3")], "C15", "#Allowed"),
            ([("C8", "n", "2.5"), ("C10", "n", "2.4")], "C10", "#Default #Min"),
            ([("C11", "inlineStr", "0.1|0.100000001")], "C11", "duplicate float32"),
            ([("C11", "inlineStr", "0|-0")], "C11", "duplicate float32"),
            ([("C8", "inlineStr", "NaN")], "C8", "float32"),
            ([("C9", "inlineStr", "Infinity")], "C9", "float32"),
            ([("C10", "inlineStr", "1e-999")], "C10", "float32"),
            ([("C10", "inlineStr", "1,25")], "C10", "float32"),
        ]
        for edits, address, reason in cases:
            with self.subTest(edits=edits):
                self.edit_cells(scalar + edits)
                self.assert_value_failure(address, reason, prior)

    def test_string_allowed_and_default_preserve_spaces_and_compare_exactly(self):
        scalar = [("C6", "inlineStr", "string"), ("C7", "inlineStr", "false"),
                  ("C11", "inlineStr", " Fire |Ice")]
        self.edit_cells(scalar + [("C10", "inlineStr", " Fire "), ("C15", "inlineStr", "")])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual(rows[9][2], " Fire ")
        self.assertEqual(rows[14][2], "")
        prior = self.csv.read_bytes()
        for address, value, reason in [("C15", "Fire", "#Allowed"), ("C15", "ice", "#Allowed"),
                                       ("C10", "Fire", "#Default #Allowed"), ("C11", "Ice|Ice", "duplicate string"),
                                       ("C11", "Fire||Ice", "invalid"), ("C8", "1", "#Min"), ("C9", "2", "#Max")]:
            with self.subTest(address=address, value=value):
                self.edit_cells(scalar + [("C15", "inlineStr", "Ice"), (address, "inlineStr", value)])
                self.assert_value_failure(address, reason, prior)

    def test_string_requires_text_and_respects_required(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        for cell_type, value, reason in [("n", "123", "text Excel cell"), ("b", "1", "text Excel cell"),
                                          ("inlineStr", "", "#Required")]:
            with self.subTest(cell_type=cell_type, value=value):
                self.edit_cells([("C6", "inlineStr", "string"), ("C15", cell_type, value)])
                self.assert_value_failure("C15", reason, prior)

    def test_string_default_and_allowed_require_text_cells(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        for address, label in [("C10", "#Default"), ("C11", "#Allowed")]:
            with self.subTest(address=address):
                self.edit_cells([("C6", "inlineStr", "string"), ("C7", "inlineStr", "false"),
                                 ("C15", "inlineStr", "123"), (address, "n", "123")])
                self.assert_value_failure(address, label + " requires a text Excel cell", prior)

    def write_reference_target(self, key="42"):
        target = self.input / "z_Profiles.xlsx"
        self.edit_cells([("B1", "inlineStr", "OtherProfile"), ("B15", "n", key)], target, "OtherProfile")

    def test_forward_reference_uses_workbook_sheet_and_pk_independent_of_file_order(self):
        self.write_reference_target()
        reference = "z_Profiles.OtherProfile.PlayerMovementDataId"
        self.edit_cells([("C12", "inlineStr", reference), ("C15", "n", "42")])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual(rows[11][2], reference)
        self.assertEqual(rows[14][2], "42")
        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.assertEqual(set(prior), {"PlayerMovement.csv", "OtherProfile.csv"})

        self.xlsx.rename(self.input / "zz_source.xlsx")
        self.run_generator()
        self.assertEqual(self.read_rows()[2][:2], ["#Workbook", "zz_source"])
        new = (self.output / "PlayerMovement.csv").read_bytes()
        self.assertEqual(new.replace(b"#Workbook,zz_source,", b"#Workbook," + self.xlsx.stem.encode() + b","),
                         prior["PlayerMovement.csv"])
        self.assertEqual((self.output / "OtherProfile.csv").read_bytes(), prior["OtherProfile.csv"])

    def test_same_workbook_accepts_mutual_and_self_references(self):
        self.write_two_sheet_workbook()
        forward = f"{self.xlsx.stem}.OtherProfile.PlayerMovementDataId"
        backward = f"{self.xlsx.stem}.PlayerMovement.PlayerMovementDataId"
        source = self.xlsx
        for index, (sheet, address, cell_type, value) in enumerate([
            (SHEET, "C12", "inlineStr", forward), (SHEET, "C15", "n", "42"),
            ("xl/worksheets/sheet2.xml", "B15", "n", "42"),
            ("xl/worksheets/sheet2.xml", "C12", "inlineStr", backward),
            ("xl/worksheets/sheet2.xml", "C15", "n", "1"),
            ("xl/worksheets/sheet2.xml", "D12", "inlineStr", forward),
            ("xl/worksheets/sheet2.xml", "D15", "n", "42"),
        ]):
            candidate = self.root / f"reference-{index}.xlsx"
            replace_cell(source, candidate, address, cell_type, value, sheet_path=sheet)
            source = candidate
        self.xlsx.write_bytes(source.read_bytes())
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual((rows[11][2], rows[14][2]), (forward, "42"))
        with (self.output / "OtherProfile.csv").open(encoding="utf-8", newline="") as file:
            other = list(csv.reader(file))
        self.assertEqual(other[11][2:4], [backward, forward])
        self.assertEqual(other[14][1:4], ["42", "1", "42"])

    def test_missing_reference_key_preserves_every_csv_and_creates_no_fresh_output(self):
        self.write_reference_target()
        reference = "z_Profiles.OtherProfile.PlayerMovementDataId"
        self.edit_cells([("C12", "inlineStr", reference), ("C15", "n", "42")])
        self.run_generator()
        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.assertEqual(set(prior), {"PlayerMovement.csv", "OtherProfile.csv"})

        self.write_reference_target(key="43")
        self.edit_cells([("C12", "inlineStr", reference), ("C15", "n", "42"), ("D15", "n", "36")])
        message = self.run_generator(success=False)
        self.assertIn("Player.xlsx:PlayerMovement!C15", message)
        self.assertIn(f"#Reference key 42 was not found in '{reference}'", message)
        self.assertEqual({path.name: path.read_bytes() for path in self.output.iterdir()}, prior)

        self.output = self.root / "FreshCsv"
        self.assertIn("!C15", self.run_generator(success=False))
        self.assertFalse(self.output.exists())

    def test_rejects_wrong_reference_target_even_when_optional_value_is_blank(self):
        self.write_reference_target()
        cases = [
            ("Missing.OtherProfile.PlayerMovementDataId", "uint32", "target workbook or sheet was not found"),
            ("z_Profiles.Missing.PlayerMovementDataId", "uint32", "target workbook or sheet was not found"),
            ("z-profiles.OtherProfile.PlayerMovementDataId", "uint32", "target workbook or sheet was not found"),
            ("z_Profiles.otherProfile.PlayerMovementDataId", "uint32", "target workbook or sheet was not found"),
            ("z_Profiles.OtherProfile.Missing", "uint32", "target field was not found"),
            ("z_Profiles.OtherProfile.playerMovementDataId", "uint32", "target field was not found"),
            ("z_Profiles.OtherProfile.MoveSpeedCmPerSecond", "uint32", "target field must be the primary key"),
            ("z_Profiles.OtherProfile.PlayerMovementDataId", "int32", "type must match uint32"),
            ("z_Profiles.OtherProfile.PlayerMovementDataId", "float32", "type must match uint32"),
            ("z_Profiles.OtherProfile.PlayerMovementDataId", "string", "type must match uint32"),
        ]
        for reference, field_type, reason in cases:
            with self.subTest(reference=reference, field_type=field_type):
                self.edit_cells([("C6", "inlineStr", field_type), ("C7", "inlineStr", "false"),
                                 ("C12", "inlineStr", reference), ("C15", "inlineStr", "")])
                message = self.run_generator(success=False)
                self.assertIn("Player.xlsx:PlayerMovement!C12", message)
                self.assertIn("#Reference " + reason, message)
                self.assertIn(reference, message)
                self.assertFalse(self.output.exists())

    def test_optional_reference_checks_default_without_filling_the_csv_blank(self):
        self.write_reference_target()
        reference = "z_Profiles.OtherProfile.PlayerMovementDataId"
        optional = [("C7", "inlineStr", "false"), ("C12", "inlineStr", reference)]
        for default in ("", "42"):
            with self.subTest(default=default):
                self.edit_cells(optional + [("C10", "inlineStr", default), ("C15", "inlineStr", "")])
                self.run_generator()
                rows = self.read_rows()
                self.assertEqual(rows[9][2], default)
                self.assertEqual(rows[14][2], "")

        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.edit_cells(optional + [("C10", "n", "99"), ("C15", "n", "42")])
        message = self.run_generator(success=False)
        self.assertIn("Player.xlsx:PlayerMovement!C10", message)
        self.assertIn(f"#Default #Reference key 99 was not found in '{reference}'", message)
        self.assertEqual({path.name: path.read_bytes() for path in self.output.iterdir()}, prior)

    def test_rejects_invalid_primary_key_cells(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        cases = [
            ("inlineStr", "", "#Required"), ("inlineStr", "1", "numeric Excel cell"),
            ("inlineStr", "1E+3", "numeric Excel cell"), ("inlineStr", "1,001", "numeric Excel cell"),
            ("b", "1", "numeric Excel cell"), ("n", "0", "#Key=Primary"),
            ("n", "-1", "uint32"), ("n", "1.5", "uint32"),
            ("n", "4294967296", "uint32"), ("n", "1000000000000000", "uint32"),
        ]
        for cell_type, value, reason in cases:
            with self.subTest(cell_type=cell_type, value=value):
                # 선언된 #Min 없이도 PK 0은 거절해야 한다.
                self.edit_cells([("B8", "inlineStr", ""), ("B15", cell_type, value)])
                self.assert_value_failure("B15", reason, prior)

    def test_duplicate_primary_key_reports_both_rows(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        self.edit_cells([("B16", "n", "1.0"), ("C16", "n", "450"),
                         ("D16", "n", "35"), ("E16", "n", "1000")])
        message = self.assert_value_failure("B16", "Duplicate primary key", prior)
        self.assertIn("B15", message)

    def test_data_values_must_satisfy_type_and_constraints(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        cases = [
            ([("C15", "inlineStr", "")], "#Required"),
            ([("C15", "inlineStr", "450")], "numeric Excel cell"),
            ([("C15", "n", "0.25")], "uint32"),
            ([("C8", "n", "451")], "#Min"),
            ([("C9", "n", "449")], "#Max"),
            ([("C11", "inlineStr", "10|20")], "#Allowed"),
            ([("C6", "inlineStr", "int32"), ("C15", "n", "-2147483649")], "int32"),
            ([("C6", "inlineStr", "int32"), ("C15", "n", "2147483648")], "int32"),
        ]
        for edits, reason in cases:
            with self.subTest(edits=edits):
                self.edit_cells(edits)
                self.assert_value_failure("C15", reason, prior)

    def test_default_is_validated_even_when_the_row_has_a_value(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        for address, limit, reason in [("C8", "400", "#Min"), ("C9", "500", "#Max"),
                                       ("C11", "450|460", "#Allowed")]:
            with self.subTest(address=address):
                default = "350" if address != "C9" else "501"
                self.edit_cells([("C7", "inlineStr", "false"), ("C10", "n", default),
                                 (address, "inlineStr", limit)])
                self.assert_value_failure("C10", f"#Default {reason}", prior)

    def test_integer_endpoints_and_optional_excel_blanks(self):
        self.edit_cells([
            ("B15", "n", "4294967295"), ("C6", "inlineStr", "int32"),
            ("C8", "n", "-2147483648"), ("C9", "n", "2147483647"),
            ("C11", "inlineStr", "-2147483648|2147483647"), ("C15", "n", "-2147483648"),
            ("D7", "inlineStr", "false"), ("D10", "n", "0"), ("D11", "inlineStr", "0"),
            ("D15", "inlineStr", ""), ("E7", "inlineStr", "false"), ("E15", "inlineStr", ""),
            ("B17", "n", "1"), ("C17", "n", "2147483647"), ("D17", "n", "0"),
        ])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual(rows[9][3], "0")
        self.assertEqual(rows[14:], [["", "4294967295", "-2147483648", "", ""],
                                     ["", "", "", "", ""], ["", "1", "2147483647", "0", ""]])

    def test_invalid_later_workbook_prevents_all_csv_writes(self):
        other = self.input / "z_other.xlsx"
        self.edit_cells([("B1", "inlineStr", "OtherProfile")], other, "OtherProfile")
        self.run_generator()
        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.assertEqual(set(prior), {"PlayerMovement.csv", "OtherProfile.csv"})
        self.edit_cells([("C15", "n", "451")])
        self.edit_cells([("B1", "inlineStr", "OtherProfile"), ("B15", "n", "0")], other, "OtherProfile")
        message = self.run_generator(success=False)
        self.assertIn("z_other.xlsx:OtherProfile!B15", message)
        self.assertIn("#Key=Primary", message)
        self.assertEqual({path.name: path.read_bytes() for path in self.output.iterdir()}, prior)
        self.output = self.root / "FreshCsv"
        self.run_generator(success=False)
        self.assertFalse(self.output.exists())

    def test_unreadable_workbook_names_the_file_and_preserves_existing_csv(self):
        self.run_generator()
        prior = self.csv.read_bytes()
        (self.input / "z-corrupt.xlsx").write_bytes(b"not an xlsx package")
        message = self.run_generator(success=False)
        self.assertIn("z-corrupt.xlsx", message)
        self.assertEqual(self.csv.read_bytes(), prior)


if __name__ == "__main__":
    unittest.main()
