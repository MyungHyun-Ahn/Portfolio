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
        first = self.root / "first.xlsx"
        replace_cell(WORKBOOK, first, "C11", "inlineStr", "10|20|450")
        replace_cell(first, self.xlsx, "E13", "inlineStr", "")
        self.run_generator()
        expected = [row[:] for row in ROWS]
        expected[10][2] = "10|20|450"
        expected[12][4] = ""
        self.assertEqual(self.read_rows(), expected)

    def write_reference_target(self, key="42"):
        target = self.input / "z-Profiles.xlsx"
        self.edit_cells([("B1", "inlineStr", "OtherProfile"), ("B15", "n", key)], target, "OtherProfile")

    def test_forward_reference_uses_workbook_sheet_and_pk_independent_of_file_order(self):
        self.write_reference_target()
        reference = "z-Profiles.OtherProfile.PlayerMovementDataId"
        self.edit_cells([("C12", "inlineStr", reference), ("C15", "n", "42")])
        self.run_generator()
        rows = self.read_rows()
        self.assertEqual(rows[11][2], reference)
        self.assertEqual(rows[14][2], "42")
        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.assertEqual(set(prior), {"PlayerMovement.csv", "OtherProfile.csv"})

        self.xlsx.rename(self.input / "zz-source.xlsx")
        self.run_generator()
        self.assertEqual({path.name: path.read_bytes() for path in self.output.iterdir()}, prior)

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
        reference = "z-Profiles.OtherProfile.PlayerMovementDataId"
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
            ("z-Profiles.Missing.PlayerMovementDataId", "uint32", "target workbook or sheet was not found"),
            ("z-profiles.OtherProfile.PlayerMovementDataId", "uint32", "target workbook or sheet was not found"),
            ("z-Profiles.otherProfile.PlayerMovementDataId", "uint32", "target workbook or sheet was not found"),
            ("z-Profiles.OtherProfile.Missing", "uint32", "target field was not found"),
            ("z-Profiles.OtherProfile.playerMovementDataId", "uint32", "target field was not found"),
            ("z-Profiles.OtherProfile.MoveSpeedCmPerSecond", "uint32", "target field must be the primary key"),
            ("z-Profiles.OtherProfile.PlayerMovementDataId", "int32", "type must match uint32"),
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
        reference = "z-Profiles.OtherProfile.PlayerMovementDataId"
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
        other = self.input / "z-other.xlsx"
        self.edit_cells([("B1", "inlineStr", "OtherProfile")], other, "OtherProfile")
        self.run_generator()
        prior = {path.name: path.read_bytes() for path in self.output.iterdir()}
        self.assertEqual(set(prior), {"PlayerMovement.csv", "OtherProfile.csv"})
        self.edit_cells([("C15", "n", "451")])
        self.edit_cells([("B1", "inlineStr", "OtherProfile"), ("B15", "n", "0")], other, "OtherProfile")
        message = self.run_generator(success=False)
        self.assertIn("z-other.xlsx:OtherProfile!B15", message)
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
