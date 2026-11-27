import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'Tools/DBGenerator'
SCHEMA = 'GameDB/Player/Character.yaml'
HEADER = 'Server/GameDB/Player/SDbCharacter.h'


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        scratch = tempfile.TemporaryDirectory(prefix='db generator ')
        self.addCleanup(scratch.cleanup)
        self.root = Path(scratch.name)
        self.tool = self.root / 'Tools/DBGenerator'
        self.tool.mkdir(parents=True)
        self.cli = self.tool / 'GenerateDb.py'
        self.database = self.root / 'Database'
        self.source = self.database / 'YamlSchema' / SCHEMA
        self.source.parent.mkdir(parents=True)
        self.schema = (ROOT / 'Database/YamlSchema' / SCHEMA).read_bytes()
        self.source.write_bytes(self.schema)
        self.output = self.database / 'Generated' / HEADER
        shutil.copyfile(TOOL / self.cli.name, self.cli)

    def generate(self, schema=SCHEMA):
        return subprocess.run([sys.executable, '-B', str(self.cli), schema], cwd=self.root,
                              capture_output=True, text=True, encoding='utf-8')

    def verify_success(self):
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stderr)
        return self.output.read_bytes()

    def test_schema_produces_server_header_and_regenerates_identical_bytes(self):
        original = self.verify_success()
        self.assertEqual(original, (ROOT / 'Database/Generated' / HEADER).read_bytes())
        self.assertIn(b'namespace Database::GameDB::Player\n', original)
        self.assertIn(b'struct SDbCharacter\n', original)
        self.assertEqual(self.verify_success(), original)
        self.assertEqual(self.source.read_bytes(), self.schema)
        self.assertEqual([p.relative_to(self.database / 'Generated').as_posix()
                          for p in (self.database / 'Generated').rglob('*') if p.is_file()], [HEADER])

    def test_name_comes_from_record_and_account_db_stays_separate(self):
        account = self.database / 'YamlSchema/AccountDB/Player/Character.yaml'
        account.parent.mkdir(parents=True)
        account.write_bytes(self.schema.replace(b'SDbCharacter', b'SDbAccountFixture'))
        before = self.verify_success()
        result = self.generate('AccountDB/Player/Character.yaml')
        self.assertEqual(result.returncode, 0, result.stderr)
        header = self.database / 'Generated/Server/AccountDB/Player/SDbAccountFixture.h'
        self.assertIn(b'namespace Database::AccountDB::Player', header.read_bytes())
        self.assertIn(b'struct SDbAccountFixture', header.read_bytes())
        self.assertEqual(self.output.read_bytes(), before)

    def test_valid_edit_replaces_owned_header(self):
        before = self.verify_success()
        self.source.write_bytes(self.schema.replace(b'm_accountId', b'm_ownerId'))
        after = self.verify_success()
        self.assertNotEqual(before, after)
        self.assertIn(b'm_ownerId', after)
        self.assertNotIn(b'm_accountId', after)

    def test_crlf_header_remains_owned_and_regenerates_as_lf(self):
        before = self.verify_success()
        self.output.write_bytes(before.replace(b'\n', b'\r\n'))
        self.assertEqual(self.verify_success(), before)

    @unittest.skipUnless(os.name == 'nt', 'Windows file replacement semantics')
    def test_read_only_output_is_preserved_when_replace_fails(self):
        before = self.verify_success()
        self.source.write_bytes(self.schema.replace(b'm_accountId', b'm_ownerId'))
        self.output.chmod(stat.S_IREAD)
        try:
            result = self.generate()
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(self.output.read_bytes(), before)
            self.assertEqual(list(self.output.parent.iterdir()), [self.output])
        finally:
            self.output.chmod(stat.S_IWRITE)
        self.assertIn(b'm_ownerId', self.verify_success())

    def test_invalid_schema_preserves_previous_output(self):
        before = self.verify_success()
        stat = self.output.stat().st_mtime_ns
        cases = (
            (b'record: SDbCharacter', b'record: SDbCharacter\nrecord: SDbOther', 'duplicate YAML key'),
            (b'table: characters', b'table: characters\nextra: invalid', 'unknown key'),
            (b'record: SDbCharacter', b'record: SDb../Bad', 'record'),
            (b'field: m_accountId', b'field: m_characterId', 'duplicate field'),
            (b'name: account_id', b'name: character_id', 'duplicate column'),
            (b'cpp_type: uint32', b'cpp_type: string', 'cpp_type'),
            (b'mysql_type: "INT UNSIGNED"', b'mysql_type: "BIGINT UNSIGNED"', 'range mismatch'),
            (b'nullable: true', b'nullable: 1', 'nullable'),
            (b', nullable: true', b'', 'nullable'),
            (b'[character_id]', b'[missing_id]', 'primary_key'),
            (b'[character_id]', b'[last_logout_map]', 'primary_key'),
            (b'default: null', b'default: 0', 'default'),
            (b'engine: InnoDB', b'engine: bogus', 'options'),
            (b'table: characters', b'table: &table characters', 'YAML anchors'),
        )
        for old, new, diagnostic in cases:
            with self.subTest(diagnostic=diagnostic, replacement=new):
                # 잘못된 입력의 다른 필드도 바꿔 부분 반영 여부를 확인한다.
                self.source.write_bytes(self.schema.replace(old, new).replace(b'm_lastLogoutMap', b'm_changedMap'))
                result = self.generate()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(diagnostic, result.stderr)
                self.assertIn('Character.yaml', result.stderr)
                self.assertEqual((self.output.read_bytes(), self.output.stat().st_mtime_ns), (before, stat))
                self.assertEqual(list(self.output.parent.iterdir()), [self.output])

    def test_invalid_input_creates_no_output(self):
        self.source.write_bytes(b'record: [unterminated\n')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Character.yaml', result.stderr)
        self.assertFalse((self.database / 'Generated').exists())

    def test_selector_cannot_escape_or_use_invalid_directory_names(self):
        before = self.verify_success()
        for selector in ('../GameDB/Player/Character.yaml', 'GameDB/Con/Character.yaml',
                         'C:/GameDB/Player/Character.yaml', 'GameDB/Player/Character.h'):
            with self.subTest(selector=selector):
                result = self.generate(selector)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('schema path', result.stderr)
                self.assertEqual(self.output.read_bytes(), before)

    @unittest.skipUnless(os.name == 'nt', 'Windows normally resolves case aliases')
    def test_case_alias_cannot_generate_different_namespace(self):
        for selector in ('Gamedb/Player/Character.yaml', 'GameDB/Player/character.yaml'):
            with self.subTest(selector=selector):
                result = self.generate(selector)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('schema path', result.stderr)
                self.assertFalse((self.database / 'Generated').exists())
        self.verify_success()

    def test_output_cannot_be_overwritten_by_another_schema(self):
        before = self.verify_success()
        other = self.source.with_name('Other.yaml')
        for record in (b'SDbCharacter', b'SDbCHARACTER'):
            with self.subTest(record=record):
                other.write_bytes(self.schema.replace(b'SDbCharacter', record).replace(b'm_accountId', b'm_ownerId'))
                result = self.generate('GameDB/Player/Other.yaml')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('output collision', result.stderr)
                self.assertEqual(self.output.read_bytes(), before)
                self.assertEqual(list(self.output.parent.iterdir()), [self.output])

    def test_unowned_header_is_not_overwritten(self):
        self.output.parent.mkdir(parents=True)
        handwritten = b'// handwritten header\n'
        self.output.write_bytes(handwritten)
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('output collision', result.stderr)
        self.assertEqual(self.output.read_bytes(), handwritten)

    @unittest.skipUnless(os.name == 'nt', 'BAT requires Windows')
    def test_batch_uses_default_schema_and_reports_failure(self):
        batch = self.tool / 'GenerateDb.bat'
        shutil.copyfile(TOOL / batch.name, batch)
        env = dict(os.environ, DB_GENERATOR_PYTHON=sys.executable)
        for args, code in ((' "" "" extra', 2), (' ""', 1), ('', 0),
                           (' GameDB/Player/Missing.yaml', 1), (' a b', 2)):
            with self.subTest(args=args):
                result = subprocess.run(f'cmd.exe /d /c ""{batch}"{args}"', cwd=self.root, env=env,
                                        capture_output=True, text=True, encoding='utf-8')
                self.assertEqual(result.returncode, code, result.stdout + result.stderr)
                self.assertEqual(self.output.is_file(), args in ('', ' GameDB/Player/Missing.yaml', ' a b'))


if __name__ == '__main__':
    unittest.main()
