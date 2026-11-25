import csv
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import unittest

from test_excel_to_csv import GENERATOR, ROOT, ROWS


def write_csv(path, rows):
    with path.open('w', encoding='utf-8', newline='') as file:
        csv.writer(file).writerows(rows)


def binary_header(name, target, schema, row_count):
    table = name.encode('utf-8')
    return (struct.pack('<4sHBI', b'GDAT', 1, target, len(table)) + table
            + hashlib.sha256(schema.encode('utf-8')).digest() + struct.pack('<I', row_count))


class CsvToArtifactsTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(GENERATOR.is_file(), 'GameDataGenerator Release CLI가 아직 빌드되지 않았습니다.')
        temp = tempfile.TemporaryDirectory(prefix='game-data-artifacts-', dir=os.environ.get('TMPDIR'))
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.input = self.root / 'Csv'
        self.input.mkdir()
        self.output = self.root / 'Generated'
        self.player = self.input / 'PlayerMovement.csv'
        self.player.write_bytes((ROOT / 'GameData/Csv/PlayerMovement.csv').read_bytes())

    def run_emit(self, *names, success=True):
        result = subprocess.run(['dotnet', str(GENERATOR), '--emit', str(self.input), str(self.output), *names],
                                cwd=self.root, capture_output=True, check=False, timeout=20)
        error = result.stderr.decode('utf-8', errors='replace')
        self.assertEqual(result.returncode, 0 if success else 1, (result.stdout, error))
        return error

    def snapshot(self):
        return {p.relative_to(self.output).as_posix(): p.read_bytes() for p in self.output.rglob('*') if p.is_file()}

    def test_player_artifacts_use_target_field_order_and_exact_binary(self):
        (self.input / 'Stale.csv').write_bytes(b'not a selected table')
        self.run_emit('PlayerMovement')
        outputs = self.snapshot()
        self.assertEqual(set(outputs), {'Common/GameDataEnums.h', 'Server/PlayerMovement.h', 'Server/PlayerMovement.sdat',
                                        'Client/PlayerMovement.h', 'Client/PlayerMovement.dat'})
        server_schema = 'PlayerMovementDataId:uint32:required\nMoveSpeedCmPerSecond:uint32:required\nCollisionRadiusCm:uint32:required\n'
        client_schema = 'PlayerMovementDataId:uint32:required\nVisualScalePermille:uint32:required\n'
        self.assertEqual(outputs['Server/PlayerMovement.sdat'], binary_header('PlayerMovement', 1, server_schema, 1)
                         + struct.pack('<III', 1, 450, 35))
        self.assertEqual(outputs['Client/PlayerMovement.dat'], binary_header('PlayerMovement', 2, client_schema, 1)
                         + struct.pack('<II', 1, 1000))
        server = outputs['Server/PlayerMovement.h'].decode('utf-8')
        client = outputs['Client/PlayerMovement.h'].decode('utf-8')
        for header, schema in [(server, server_schema), (client, client_schema)]:
            self.assertIn('\n\tnamespace PlayerMovementFormat\n', header)
            row, metadata = header.split('\n\tnamespace PlayerMovementFormat\n', maxsplit=1)
            self.assertIn('struct SPlayerMovementRow', row)
            self.assertIn('GetPlayerMovementDataId() const noexcept', row)
            for name in ('kFormatVersion', 'kTarget', 'kTableName', 'kSchemaHash'):
                self.assertNotIn(name, row)
                self.assertIn(name, metadata)
            encoded = bytes(int(value, 16) for value in re.findall(r'0x([a-f0-9]{2})', header))
            self.assertEqual(encoded, hashlib.sha256(schema.encode()).digest())
        self.assertLess(server.index('m_moveSpeedCmPerSecond'), server.index('m_collisionRadiusCm'))
        self.assertNotIn('m_moveSpeedCmPerSecond', client)
        self.assertNotIn('m_visualScalePermille', server)
        self.assertIn('GetMoveSpeedCmPerSecond() const noexcept', server)
        self.assertIn('GetCollisionRadiusCm() const noexcept', server)
        self.assertIn('GetVisualScalePermille() const noexcept', client)
        self.assertNotIn('GetMoveSpeedCmPerSecond', client)
        self.assertNotIn('GetCollisionRadiusCm', client)
        self.assertNotIn('GetVisualScalePermille', server)
        self.run_emit('PlayerMovement')
        self.assertEqual(self.snapshot(), outputs)
        rows = [row.copy() for row in ROWS]
        rows[14][2] = '451'
        write_csv(self.player, rows)
        self.run_emit('PlayerMovement')
        changed = self.snapshot()
        self.assertEqual({key for key in changed if changed[key] != outputs[key]}, {'Server/PlayerMovement.sdat'})
        self.assertEqual(changed['Server/PlayerMovement.sdat'][-12:], struct.pack('<III', 1, 451, 35))

    def test_array_enum_string_and_optional_values_have_explicit_encoding(self):
        write_csv(self.input / 'Enum.csv', [['Type', 'Name', 'Value'], ['DamageType', 'Fire', '9'], ['DamageType', 'Ice', '2']])
        rows = [row.copy() for row in ROWS]
        rows[5][2:] = ['enum:DamageType[]', 'string', 'float32[]']
        rows[6][2:] = ['true', 'false', 'false']
        rows[9][3] = '검, "방패"\n2행'
        rows[14][2:] = ['[Fire | Ice]', '', '[]']
        rows.append(['', '2', '[]', '활', ''])
        rows.append(['', '3', '[]', '활', '[-1.5 | 2.25]'])
        write_csv(self.player, rows)
        self.run_emit('Enum', 'PlayerMovement')
        outputs = self.snapshot()
        server_schema = ('PlayerMovementDataId:uint32:required\nMoveSpeedCmPerSecond:enum:DamageType[]:required\n'
                         'CollisionRadiusCm:string:optional\nenum:DamageType:Fire:9\nenum:DamageType:Ice:2\n')
        client_schema = 'PlayerMovementDataId:uint32:required\nVisualScalePermille:float32[]:optional\n'
        text = rows[9][3].encode('utf-8')
        bow = '활'.encode('utf-8')
        server_rows = (struct.pack('<IIIIBI', 1, 2, 9, 2, 1, len(text)) + text
                       + struct.pack('<IIBI', 2, 0, 1, len(bow)) + bow
                       + struct.pack('<IIBI', 3, 0, 1, len(bow)) + bow)
        client_rows = struct.pack('<IBIIB', 1, 1, 0, 2, 0) + struct.pack('<IBIff', 3, 1, 2, -1.5, 2.25)
        self.assertEqual(outputs['Server/PlayerMovement.sdat'], binary_header('PlayerMovement', 1, server_schema, 3) + server_rows)
        self.assertEqual(outputs['Client/PlayerMovement.dat'], binary_header('PlayerMovement', 2, client_schema, 3) + client_rows)
        self.assertIn(b'enum class DamageType : std::uint32_t', outputs['Common/GameDataEnums.h'])
        self.assertIn(b'Fire = 9u,', outputs['Common/GameDataEnums.h'])
        self.assertIn(b'std::optional<std::string>', outputs['Server/PlayerMovement.h'])
        self.assertIn(b'std::optional<std::vector<float>>', outputs['Client/PlayerMovement.h'])
        if os.environ.get('GAME_DATA_TEST_MSVC') == '1':
            result = subprocess.run(['cl.exe', '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/W4', '/WX',
                                     '/Fe:GameDataHeaders.exe',
                                     f'/I{self.output}', f'/I{ROOT / "Common"}',
                                     str(Path(__file__).with_name('GameDataHeaders.cpp'))],
                                    cwd=self.root, capture_output=True, timeout=45)
            self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
            result = subprocess.run([self.root / 'GameDataHeaders.exe'], cwd=self.root, capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
        self.assertEqual(self.run_emit('PlayerMovement', success=False).split(':', 1)[0], 'PlayerMovement.csv!C6')
        self.assertEqual(self.snapshot(), outputs)

    def test_invalid_projected_reference_and_bad_csv_preserve_outputs(self):
        self.run_emit('PlayerMovement')
        before = self.snapshot()
        rows = [row.copy() for row in ROWS]
        rows[0][1] = 'ServerOnly'
        rows[1][1] = 'Server'
        write_csv(self.input / 'ServerOnly.csv', rows)
        rows = [row.copy() for row in ROWS]
        rows[11][4] = 'Profiles.ServerOnly.PlayerMovementDataId'
        rows[14][4] = '1'
        write_csv(self.player, rows)
        error = self.run_emit('PlayerMovement', 'ServerOnly', success=False)
        self.assertIn('PlayerMovement.csv!E12', error)
        self.assertIn('Client', error)
        self.assertEqual(self.snapshot(), before)
        self.player.write_bytes(b'#Table,"unterminated')
        error = self.run_emit('PlayerMovement', success=False)
        self.assertIn('PlayerMovement.csv', error)
        self.assertIn('quoted', error)
        self.assertEqual(self.snapshot(), before)


    def test_bad_enum_and_invalid_utf8_are_rejected_before_any_output(self):
        enum_path = self.input / 'Enum.csv'
        for rows, reason in [
            ([['DamageType', 'Fire', '1'], ['DamageType', 'Ice', '1']], 'Duplicate enum value'),
            ([['DamageType', 'Fire', '1\0']], 'uint32'),
            ([['Server', 'Fire', '1']], 'GameData namespace'),
            ([['DamageType', 'return', '1']], 'Invalid enum member'),
        ]:
            with self.subTest(rows=rows):
                write_csv(enum_path, [['Type', 'Name', 'Value'], *rows])
                self.assertIn(reason, self.run_emit('Enum', 'PlayerMovement', success=False))
                self.assertFalse(self.output.exists())
        self.player.write_bytes(b'\xff')
        self.assertIn('PlayerMovement.csv: Invalid UTF-8', self.run_emit('PlayerMovement', success=False))
        self.assertFalse(self.output.exists())

    def test_changing_target_does_not_leave_a_stale_client_artifact(self):
        self.run_emit('PlayerMovement')
        before = self.snapshot()
        rows = [row.copy() for row in ROWS]
        rows[1][1] = 'Server'
        write_csv(self.player, rows)
        error = self.run_emit('PlayerMovement', success=False)
        self.assertIn('Stale artifact', error)
        self.assertIn('Client', error)
        self.assertEqual(self.snapshot(), before)
        self.output = self.root / 'ServerOnlyOutput'
        self.run_emit('PlayerMovement')
        self.assertEqual(set(self.snapshot()), {'Common/GameDataEnums.h', 'Server/PlayerMovement.h', 'Server/PlayerMovement.sdat'})

    def test_enum_cpp_macro_names_are_rejected_before_replacing_outputs(self):
        enum_path = self.input / 'Enum.csv'
        write_csv(enum_path, [['Type', 'Name', 'Value'], ['DamageType', 'Fire', '9']])
        self.run_emit('Enum', 'PlayerMovement')
        before = self.snapshot()
        for name, location in [('UINT32_MAX', 'B2'), ('INT_FAST64_MIN', 'B2'), ('SIZE_MAX', 'A2'), ('ERROR', 'B2')]:
            with self.subTest(name=name):
                row = [name, 'Fire', '9'] if location == 'A2' else ['DamageType', name, '9']
                write_csv(enum_path, [['Type', 'Name', 'Value'], row])
                error = self.run_emit('Enum', 'PlayerMovement', success=False)
                self.assertIn(f'Enum.csv!{location}', error)
                self.assertIn('Invalid enum', error)
                self.assertEqual(self.snapshot(), before)

    def test_windows_getter_macro_names_are_rejected_before_replacing_outputs(self):
        self.run_emit('PlayerMovement')
        before = self.snapshot()
        for name in ('message', 'Object', 'RValue', 'GValue', 'BValue'):
            with self.subTest(name=name):
                rows = [row.copy() for row in ROWS]
                rows[13][2] = name
                write_csv(self.player, rows)
                error = self.run_emit('PlayerMovement', success=False)
                self.assertIn('PlayerMovement.csv!C14', error)
                self.assertIn(f'Get{name[0].upper()}{name[1:]}', error)
                self.assertIn('Windows macro', error)
                self.assertEqual(self.snapshot(), before)

    def test_csv_must_supply_numeric_unique_key_and_valid_cpp_names(self):
        for row, column, value, location, reason in [
            (14, 1, '1.0', 'B15', 'valid uint32'), (14, 1, '0', 'B15', 'at least 1'),
            (13, 2, 'class', 'C14', '#Field'), (13, 3, 'moveSpeedCmPerSecond', 'D14', 'duplicate C++'),
        ]:
            with self.subTest(value=value):
                rows = [item.copy() for item in ROWS]
                rows[row][column] = value
                write_csv(self.player, rows)
                error = self.run_emit('PlayerMovement', success=False)
                self.assertIn(f'PlayerMovement.csv!{location}', error)
                self.assertIn(reason, error)
                self.assertFalse(self.output.exists())
        write_csv(self.player, [*ROWS, ROWS[-1]])
        error = self.run_emit('PlayerMovement', success=False)
        self.assertIn('PlayerMovement.csv!B16', error)
        self.assertIn('Duplicate primary key 1', error)
        self.assertIn('PlayerMovement.csv!B15', error)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
