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
        self.tables = self.root / 'Tables'
        self.player = self.input / 'PlayerMovement.csv'
        self.player.write_bytes((ROOT / 'GameData/Csv/PlayerMovement.csv').read_bytes())

    def run_emit(self, *names, success=True):
        result = subprocess.run(['dotnet', str(GENERATOR), '--emit', str(self.input), str(self.output), *names],
                                cwd=self.root, capture_output=True, check=False, timeout=20)
        error = result.stderr.decode('utf-8', errors='replace')
        self.assertEqual(result.returncode, 0 if success else 1, (result.stdout, error))
        return error

    def snapshot(self, directory=None):
        root = self.output if directory is None else directory
        return {p.relative_to(root).as_posix(): p.read_bytes() for p in root.rglob('*') if p.is_file()}

    def test_workbook_metadata_drives_paths_and_namespace_without_excel(self):
        rows = [row.copy() for row in ROWS]
        write_csv(self.player, rows)
        self.run_emit('PlayerMovement')
        before = self.snapshot()
        manual_before = self.snapshot(self.tables)
        for name in ('', '../Player', 'class', 'NUL', 'ERROR', 'MAX_PATH', 'CGameDataTables', 'Enum'):
            with self.subTest(name=name):
                rows[2][1] = name
                write_csv(self.player, rows)
                self.assertIn('PlayerMovement.csv!B3: Invalid #Workbook', self.run_emit('PlayerMovement', success=False))
                self.assertEqual(self.snapshot(), before)
                self.assertEqual(self.snapshot(self.tables), manual_before)
        rows[2][1] = 'Characters'
        write_csv(self.player, rows)
        self.assertIn('Stale artifact', self.run_emit('PlayerMovement', success=False))
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(self.snapshot(self.tables), manual_before)
        self.output = self.root / 'Regenerated'
        self.run_emit('PlayerMovement')
        after = self.snapshot()
        self.assertIn(b'namespace GameData::Client::Characters', after['Client/Characters/PlayerMovement.h'])
        self.assertIn(b'namespace GameData::Client::Characters', after['Client/Characters/PlayerMovement.TableBase.g.h'])
        self.assertEqual(after['Data/Client/Characters/PlayerMovement.cdat'], before['Data/Client/Player/PlayerMovement.cdat'])
        self.assertEqual(self.snapshot(self.tables), manual_before)
        rows[0][1] = 'OtherProfile'
        rows[2][1] = 'characters'
        write_csv(self.input / 'OtherProfile.csv', rows)
        self.assertIn('#Workbook', self.run_emit('PlayerMovement', 'OtherProfile', success=False))
        self.assertEqual(self.snapshot(), after)

    def test_player_artifacts_use_target_field_order_and_exact_binary(self):
        (self.input / 'Stale.csv').write_bytes(b'not a selected table')
        self.run_emit('PlayerMovement')
        outputs = self.snapshot()
        self.assertEqual(set(outputs), {'Common/GameDataEnums.h', 'Server/Player/PlayerMovement.h', 'Client/Player/PlayerMovement.h',
                                        'Server/Player/PlayerMovement.TableBase.g.h', 'Client/Player/PlayerMovement.TableBase.g.h',
                                        'Server/GameDataAll.g.h', 'Client/GameDataAll.g.h',
                                        'Server/CGameDataTables.g.h', 'Client/CGameDataTables.g.h',
                                        'Data/Server/Player/PlayerMovement.sdat', 'Data/Client/Player/PlayerMovement.cdat'})
        server_schema = 'PlayerMovementDataId:uint32:required\nMoveSpeedCmPerSecond:uint32:required\nCollisionRadiusCm:uint32:required\n'
        client_schema = 'PlayerMovementDataId:uint32:required\nVisualScalePermille:uint32:required\n'
        self.assertEqual(outputs['Data/Server/Player/PlayerMovement.sdat'], binary_header('PlayerMovement', 1, server_schema, 1)
                         + struct.pack('<III', 1, 450, 35))
        self.assertEqual(outputs['Data/Client/Player/PlayerMovement.cdat'], binary_header('PlayerMovement', 2, client_schema, 1)
                         + struct.pack('<II', 1, 1000))
        server = outputs['Server/Player/PlayerMovement.h'].decode('utf-8')
        client = outputs['Client/Player/PlayerMovement.h'].decode('utf-8')
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
        self.assertEqual({key for key in changed if changed[key] != outputs[key]}, {'Data/Server/Player/PlayerMovement.sdat'})
        self.assertEqual(changed['Data/Server/Player/PlayerMovement.sdat'][-12:], struct.pack('<III', 1, 451, 35))

    def test_manual_table_is_created_only_when_absent(self):
        manual_server = self.tables / 'Server/CPlayerMovementTable.h'
        manual_client = self.tables / 'Client/CPlayerMovementTable.h'
        manual_server.parent.mkdir(parents=True)
        sentinel = b'// Existing server implementation.\r\n'
        manual_server.write_bytes(sentinel)
        before = manual_server.stat().st_mtime_ns
        self.run_emit('PlayerMovement')
        self.assertEqual(manual_server.read_bytes(), sentinel)
        self.assertEqual(manual_server.stat().st_mtime_ns, before)
        client = manual_client.read_bytes()
        self.assertIn(b'Validate() const override', client)
        self.assertIn(b'Init() override', client)
        self.assertLess(client.index(b'Init() override'), client.index(b'Validate() const override'))
        self.assertNotIn(b'Load(', client)
        client_modified = client + b'// User change\n'
        manual_client.write_bytes(client_modified)
        observed = self.snapshot()
        self.run_emit('PlayerMovement')
        self.assertEqual(self.snapshot(), observed)
        self.assertEqual(manual_client.read_bytes(), client_modified)
        self.assertEqual(manual_server.read_bytes(), sentinel)

    def test_bundle_loader_uses_projected_file_names_under_one_directory(self):
        rows = [row.copy() for row in ROWS]
        rows[0][1] = 'ServerOnly'
        rows[1][1] = 'Server'
        # 로더 내부 구조체와 이름이 같아도 워크북 네임스페이스를 찾아야 한다.
        rows[2][1] = 'STableEntry'
        rows[14][2] = '725'
        write_csv(self.input / 'ServerOnly.csv', rows)
        self.run_emit('ServerOnly', 'PlayerMovement')
        outputs = self.snapshot()
        server = outputs['Server/CGameDataTables.g.h']
        client = outputs['Client/CGameDataTables.g.h']
        self.assertIn(b'"Player/PlayerMovement.sdat"', server)
        self.assertIn(b'"STableEntry/ServerOnly.sdat"', server)
        self.assertIn(b'"Player/PlayerMovement.cdat"', client)
        self.assertNotIn(b'ServerOnly', client)
        self.assertNotIn(b'ServerOnly', outputs['Client/GameDataAll.g.h'])
        self.assertNotIn(b'GameData/Generated', server + client)
        self.assertNotIn(b'pOutFailedPath', server + client)
        if os.environ.get('GAME_DATA_TEST_MSVC') == '1':
            hooks = self.tables / 'Server/CPlayerMovementTable.h'
            hooks.write_bytes(hooks.read_bytes().replace(
                b'return true;', b'const auto* pOtherRow = ::GameData::Server::STableEntry::CServerOnlyTableBase::Find(1);\n'
                b'\t\t\treturn pOtherRow != nullptr && pOtherRow->GetMoveSpeedCmPerSecond() == 725 && ++::g_hookOrder == 1;'
            ).replace(b'return false;', b'return ++::g_hookOrder == 3;'))
            hooks = self.tables / 'Server/CServerOnlyTable.h'
            hooks.write_bytes(hooks.read_bytes().replace(
                b'\tpublic:\n', b'\tpublic:\n'
                b'\t\tCServerOnlyTable()\n\t\t{\n\t\t\t::CheckRowsUnavailable();\n\t\t}\n\n'
                b'\t\t~CServerOnlyTable() override\n\t\t{\n'
                b'\t\t\t::CheckRowsUnavailable();\n\t\t\t++::g_destroyedTables;\n\t\t}\n\n'
            ).replace(b'return true;', b'return ++::g_hookOrder == 2 && ::g_failAt != 2;'
            ).replace(b'return false;', b'return ++::g_hookOrder == 4 && ::g_failAt != 4;'))
            hooks = self.tables / 'Client/CPlayerMovementTable.h'
            hooks.write_bytes(hooks.read_bytes().replace(b'return false;', b'return Find(1) != nullptr;'))
            for target in ('Server', 'Client'):
                relocated = self.root / f'{target} data'
                source = self.output / 'Data' / target
                for data in source.rglob('*'):
                    if data.is_file():
                        destination = relocated / data.relative_to(source)
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        destination.write_bytes(data.read_bytes())
            common = ROOT / 'Common'
            mhlib = ROOT / 'Server/MHLib/includes'
            result = subprocess.run(['cl.exe', '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/W4', '/WX',
                                     '/DUNICODE', '/D_UNICODE', f'/external:I{mhlib}', '/external:W3',
                                     '/Fe:GameDataTablesTests.exe', f'/I{self.output}', f'/I{common}', f'/I{self.root}',
                                     str(Path(__file__).with_name('GameDataTablesTests.cpp')),
                                     str(common / 'GameData/CGameDataReader.cpp')],
                                    cwd=self.root, capture_output=True, timeout=45)
            self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
            result = subprocess.run([self.root / 'GameDataTablesTests.exe'], cwd=self.root, capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, f'Native bundle case {result.returncode}: {result.stdout!r} {result.stderr!r}')
            logs = list((self.root / 'GameDataLogs').rglob('*.log'))
            self.assertEqual(len(logs), 1)
            file_log = logs[0].read_text(encoding='utf-8')
            console = result.stdout.decode('utf-8')
            self.assertNotIn('[Debug]', console)
            self.assertIn('[Debug]', file_log)
            events = [line.split('event=', 1)[1].split(' | ', 1)[0] for line in file_log.splitlines() if 'event=game_data_' in line]
            second = 'target=Server stage={} file=STableEntry/ServerOnly.sdat'
            self.assertEqual(events[:3], [
                'game_data_bundle_rejected target=Client reason=empty_directory',
                'game_data_table_completed target=Server stage=load file=Player/PlayerMovement.sdat',
                'game_data_table_failed ' + second.format('load'),
            ])
            self.assertEqual([event for event in events if event.startswith('game_data_table_failed ')],
                             ['game_data_table_failed ' + second.format(stage) for stage in ('load', 'init', 'validate')])
            stages = [event for event in events if event.startswith('game_data_stage_completed ')]
            server_load = 'game_data_stage_completed target=Server stage=load tables=2'
            server_init = 'game_data_stage_completed target=Server stage=init tables=2'
            server_validate = 'game_data_stage_completed target=Server stage=validate tables=2'
            server_success = [server_load, server_init, server_validate]
            client_success = [f'game_data_stage_completed target=Client stage={stage} tables=1'
                              for stage in ('load', 'init', 'validate')]
            self.assertEqual(stages, [server_load, server_load, server_init, *server_success, *client_success, *server_success])
            self.assertEqual(console.count('[Info]'), len(stages))
            self.assertIn('game_data_bundle_rejected target=Server reason=bundle_already_exists', events)
            self.assertEqual(console.count('[Error]'), 5)
        self.assertIn(b'std::unique_ptr<IGameDataTable>', server)
        self.assertNotIn(b'm_tablePlayerMovement', server)
        self.assertNotIn(b'GetPlayerMovementTable', server)
        self.assertNotIn(b'bindTables', server + client)
        for target in ('Server', 'Client'):
            base = outputs[f'{target}/Player/PlayerMovement.TableBase.g.h']
            self.assertNotRegex(base, rb'\bsm_pTable\b')

    @unittest.skipUnless(os.environ.get('GAME_DATA_TEST_MSVC') == '1', 'requires the MSVC x64 environment')
    def test_cpp_reader_loads_player_files_and_rejects_invalid_files(self):
        generated = ROOT / 'GameData/Generated'
        server_path = 'Data/Server/Player/PlayerMovement.sdat'
        client_path = 'Data/Client/Player/PlayerMovement.cdat'
        for relative in (server_path, client_path):
            destination = self.root / relative
            destination.parent.mkdir(parents=True)
            destination.write_bytes((generated / relative).read_bytes())

        rows = [row.copy() for row in ROWS]
        rows[14] = ['', '258', '600', '70', '1200']
        rows.append(['', '1', '451', '35', '1100'])
        write_csv(self.player, rows)
        self.run_emit('PlayerMovement')
        server = (self.output / server_path).read_bytes()
        (self.root / 'updated.sdat').write_bytes(server)
        (self.root / 'updated.cdat').write_bytes((self.output / client_path).read_bytes())

        table_offset = struct.calcsize('<4sHBI')
        hash_offset = table_offset + len(b'PlayerMovement')
        count_offset = hash_offset + 32
        payload_offset = count_offset + 4
        second_key_offset = payload_offset + struct.calcsize('<III')
        invalid = {
            'magic': b'X' + server[1:],
            'version': server[:4] + struct.pack('<H', 2) + server[6:],
            'table-name': server[:table_offset] + b'X' + server[table_offset + 1:],
            'table-length': server[:7] + struct.pack('<I', 0xffffffff) + server[table_offset:],
            'schema': server[:hash_offset] + bytes([server[hash_offset] ^ 1]) + server[hash_offset + 1:],
            'short-header': server[:count_offset + 3],
            'short-row': server[:-1],
            'row-count': server[:count_offset] + struct.pack('<I', 0xffffffff) + server[payload_offset:],
            'trailing': server + b'\0',
            'zero-key': server[:payload_offset] + struct.pack('<I', 0) + server[payload_offset + 4:],
            'duplicate-key': (server[:payload_offset] + struct.pack('<I', 2) + server[payload_offset + 4:second_key_offset]
                              + struct.pack('<I', 2) + server[second_key_offset + 4:]),
        }
        for name, contents in invalid.items():
            (self.root / f'{name}.sdat').write_bytes(contents)
        (self.root / 'empty.sdat').write_bytes(server[:count_offset] + struct.pack('<I', 0))

        common = ROOT / 'Common'
        fixture = Path(__file__).with_name('PlayerMovementReaderTests.cpp')
        reader = common / 'GameData/CGameDataReader.cpp'
        result = subprocess.run(['cl.exe', '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/W4', '/WX',
                                 '/DUNICODE', '/D_UNICODE', f'/external:I{ROOT / "Server/MHLib/includes"}', '/external:W3',
                                 '/Fe:PlayerMovementReaderTests.exe', f'/I{self.output}', f'/I{common}', f'/I{ROOT / "GameData"}',
                                 str(fixture), str(reader)], cwd=self.root, capture_output=True, timeout=45)
        self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
        result = subprocess.run([self.root / 'PlayerMovementReaderTests.exe'], cwd=self.root, capture_output=True, timeout=20)
        self.assertEqual(result.returncode, 0, f'Native reader case {result.returncode}: {result.stdout!r} {result.stderr!r}')

    def test_array_enum_string_and_optional_values_have_explicit_encoding(self):
        write_csv(self.input / 'Enum.csv', [['Type', 'Name', 'Value'], ['DamageType', 'Fire', '9'], ['DamageType', 'Ice', '2']])
        rows = [row.copy() for row in ROWS]
        rows[5][2:] = ['enum:DamageType[]', 'string', 'float32[]']
        rows[6][2:] = ['true', 'false', 'false']
        rows[9][3] = '검, "방패"\n2행'
        rows[14][2:] = ['[Fire | Ice]', '', '[]']
        rows.append(['', '2', '[]', '활', ''])
        rows.append(['', '3', '[]', '활', '[-1.5 | 2.25]'])
        for row, value in zip(rows, ['', '', '', '', 'Server', 'int32', 'true', '', '', '', '', '', '', 'SignedDelta',
                                     '-2147483647', '-1', '2147483647'], strict=True):
            row.append(value)
        write_csv(self.player, rows)
        self.run_emit('Enum', 'PlayerMovement')
        outputs = self.snapshot()
        server_schema = ('PlayerMovementDataId:uint32:required\nMoveSpeedCmPerSecond:enum:DamageType[]:required\n'
                         'CollisionRadiusCm:string:optional\nSignedDelta:int32:required\n'
                         'enum:DamageType:Fire:9\nenum:DamageType:Ice:2\n')
        client_schema = 'PlayerMovementDataId:uint32:required\nVisualScalePermille:float32[]:optional\n'
        text = rows[9][3].encode('utf-8')
        bow = '활'.encode('utf-8')
        server_rows = (struct.pack('<IIIIBI', 1, 2, 9, 2, 1, len(text)) + text + struct.pack('<i', -2147483647)
                       + struct.pack('<IIBI', 2, 0, 1, len(bow)) + bow + struct.pack('<i', -1)
                       + struct.pack('<IIBI', 3, 0, 1, len(bow)) + bow + struct.pack('<i', 2147483647))
        client_rows = struct.pack('<IBIIB', 1, 1, 0, 2, 0) + struct.pack('<IBIff', 3, 1, 2, -1.5, 2.25)
        self.assertEqual(outputs['Data/Server/Player/PlayerMovement.sdat'], binary_header('PlayerMovement', 1, server_schema, 3) + server_rows)
        self.assertEqual(outputs['Data/Client/Player/PlayerMovement.cdat'], binary_header('PlayerMovement', 2, client_schema, 3) + client_rows)
        self.assertIn(b'enum class DamageType : std::uint32_t', outputs['Common/GameDataEnums.h'])
        self.assertIn(b'Fire = 9u,', outputs['Common/GameDataEnums.h'])
        self.assertIn(b'std::optional<std::string>', outputs['Server/Player/PlayerMovement.h'])
        self.assertIn(b'std::optional<std::vector<float>>', outputs['Client/Player/PlayerMovement.h'])
        if os.environ.get('GAME_DATA_TEST_MSVC') == '1':
            server = outputs['Data/Server/Player/PlayerMovement.sdat']
            client = outputs['Data/Client/Player/PlayerMovement.cdat']
            server_start = len(binary_header('PlayerMovement', 1, server_schema, 3))
            client_start = len(binary_header('PlayerMovement', 2, client_schema, 3))
            text_start = server_start + struct.calcsize('<IIIIBI')
            presence = client_start + struct.calcsize('<I')
            (self.root / 'invalid-utf8.sdat').write_bytes(server[:text_start] + b'\xff' + server[text_start + 1:])
            (self.root / 'invalid-string-size.sdat').write_bytes(server[:text_start - 4] + b'\xff' * 4 + server[text_start:])
            (self.root / 'invalid-presence.cdat').write_bytes(client[:presence] + b'\x02' + client[presence + 1:])
            (self.root / 'invalid-array-size.cdat').write_bytes(client[:presence + 1] + b'\xff' * 4 + client[presence + 5:])
            (self.root / 'invalid-float.cdat').write_bytes(client[:-4] + struct.pack('<I', 0x7f800000))
            result = subprocess.run(['cl.exe', '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/W4', '/WX',
                                     '/Fe:GameDataHeaders.exe',
                                     f'/I{self.output}', f'/I{ROOT / "Common"}', f'/I{self.root}',
                                     str(Path(__file__).with_name('GameDataHeaders.cpp')),
                                     str(ROOT / 'Common/GameData/CGameDataReader.cpp')],
                                    cwd=self.root, capture_output=True, timeout=45)
            self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
            result = subprocess.run([self.root / 'GameDataHeaders.exe'], cwd=self.root, capture_output=True, timeout=20)
            self.assertEqual(result.returncode, 0, (result.stdout, result.stderr))
        self.assertEqual(self.run_emit('PlayerMovement', success=False).split(':', 1)[0], 'PlayerMovement.csv!C6')
        self.assertEqual(self.snapshot(), outputs)

    def test_invalid_projected_reference_and_bad_csv_preserve_outputs(self):
        self.run_emit('PlayerMovement')
        before = self.snapshot()
        manual = self.tables / 'Server/CPlayerMovementTable.h'
        manual.write_bytes(manual.read_bytes() + b'// User-authored validation\n')
        manual_before = self.snapshot(self.tables)
        rows = [row.copy() for row in ROWS]
        rows[0][1] = 'ServerOnly'
        rows[1][1] = 'Server'
        rows[2][1] = 'Profiles'
        write_csv(self.input / 'ServerOnly.csv', rows)
        rows = [row.copy() for row in ROWS]
        rows[11][4] = 'Profiles.ServerOnly.PlayerMovementDataId'
        rows[14][4] = '1'
        write_csv(self.player, rows)
        error = self.run_emit('PlayerMovement', 'ServerOnly', success=False)
        self.assertIn('PlayerMovement.csv!E12', error)
        self.assertIn('Client', error)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(self.snapshot(self.tables), manual_before)
        rows[11][4] = 'WrongWorkbook.ServerOnly.PlayerMovementDataId'
        write_csv(self.player, rows)
        error = self.run_emit('PlayerMovement', 'ServerOnly', success=False)
        self.assertIn("PlayerMovement.csv!E12: #Reference target workbook must match 'Profiles'", error)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(self.snapshot(self.tables), manual_before)
        self.player.write_bytes(b'#Table,"unterminated')
        error = self.run_emit('PlayerMovement', success=False)
        self.assertIn('PlayerMovement.csv', error)
        self.assertIn('quoted', error)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(self.snapshot(self.tables), manual_before)


    def test_bad_enum_and_invalid_utf8_are_rejected_before_any_output(self):
        enum_path = self.input / 'Enum.csv'
        for rows, reason in [
            ([['DamageType', 'Fire', '1'], ['DamageType', 'Ice', '1']], 'Duplicate enum value'),
            ([['DamageType', 'Fire', '1\0']], 'uint32'),
            ([['Server', 'Fire', '1']], 'GameData namespace'),
            ([['CGameDataReader', 'Fire', '1']], 'GameData namespace'),
            ([['EGameDataReadResult', 'Fire', '1']], 'GameData namespace'),
            ([['SGameDataFormat', 'Fire', '1']], 'GameData namespace'),
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
        (self.output / 'Client/Player/PlayerMovement.h').unlink()
        (self.output / 'Client/Player/PlayerMovement.TableBase.g.h').unlink()
        (self.output / 'Client/GameDataAll.g.h').unlink()
        (self.output / 'Client/CGameDataTables.g.h').unlink()
        before = self.snapshot()
        rows = [row.copy() for row in ROWS]
        rows[1][1] = 'Server'
        write_csv(self.player, rows)
        error = self.run_emit('PlayerMovement', success=False)
        self.assertIn('Stale artifact', error)
        self.assertIn('Data/Client/Player/PlayerMovement.cdat', error.replace('\\', '/'))
        self.assertEqual(self.snapshot(), before)
        self.output = self.root / 'ServerOnlyOutput'
        self.run_emit('PlayerMovement')
        self.assertEqual(set(self.snapshot()), {'Common/GameDataEnums.h', 'Server/Player/PlayerMovement.h',
                                                'Server/Player/PlayerMovement.TableBase.g.h', 'Data/Server/Player/PlayerMovement.sdat',
                                                'Server/GameDataAll.g.h', 'Server/CGameDataTables.g.h'})
        self.assertTrue((self.tables / 'Client/CPlayerMovementTable.h').is_file())

    def test_legacy_binary_locations_are_rejected_without_replacing_outputs(self):
        for relative in ('Server/PlayerMovement.sdat', 'Client/PlayerMovement.dat'):
            with self.subTest(relative=relative):
                legacy = self.output / relative
                legacy.parent.mkdir(parents=True, exist_ok=True)
                legacy.write_bytes(b'legacy binary')
                before = self.snapshot()
                error = self.run_emit('PlayerMovement', success=False).replace('\\', '/')
                self.assertIn(f"Stale artifact '{relative}'", error)
                self.assertEqual(self.snapshot(), before)
                legacy.unlink()

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
