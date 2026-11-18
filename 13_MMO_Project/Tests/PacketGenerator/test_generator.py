import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
CLI = ROOT / 'Tools/PacketGenerator/GeneratePackets.py'
NOTI = '''packets:
  - name: PulseNoti
    kind: noti
    direction: client_to_server
    id: 7
    fields:
      - {name: account_no, type: int64}
'''


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        scratch = os.environ.get('PACKET_GENERATOR_TEST_TEMP')
        directory = tempfile.TemporaryDirectory(prefix='packet generator ', dir=scratch)
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.schema = self.root / 'schema'
        self.source = self.schema / 'System/Pulse.yaml'
        self.source.parent.mkdir(parents=True)
        self.source.write_text(NOTI, encoding='utf-8')
        self.common, self.client, self.server = (self.root / name for name in ('common', 'client', 'server'))

    def generate(self, *, schema=None, client=None):
        return subprocess.run([
            sys.executable, str(CLI), '--schema-dir', str(schema or self.schema),
            '--common-out', str(self.common), '--client-out', str(client or self.client),
            '--server-out', str(self.server),
        ], cwd=self.root, capture_output=True, text=True, encoding='utf-8')

    def assert_generated(self, result):
        self.assertEqual(result.returncode, 0, result.stderr)

    def snapshot(self):
        return {path: (path.read_bytes(), path.stat().st_mtime_ns)
                for root in (self.common, self.client, self.server) for path in root.rglob('*') if path.is_file()}

    def test_single_noti_projects_by_direction_and_identical_run_keeps_mtimes(self):
        self.assert_generated(self.generate())
        self.assertIn('SPulseNoti', (self.common / 'System/Pulse.g.h').read_text())
        self.assertIn('MakePacketPulseNoti', (self.client / 'ClientPacketSend.g.h').read_text())
        self.assertNotIn('HandlePulseNoti', (self.client / 'IClientPacketHandler.g.h').read_text())
        self.assertIn('HandlePulseNoti', (self.server / 'IServerPacketHandler.g.h').read_text())
        self.assertNotIn('MakePacketPulseNoti', (self.server / 'ServerPacketSend.g.h').read_text())
        before = self.snapshot()
        self.assert_generated(self.generate())
        self.assertEqual(before, self.snapshot())

    def test_production_packet_contract(self):
        self.assert_generated(self.generate(schema=ROOT / 'Common/Packet/Schema'))
        enum = (self.common / 'PacketTypes.g.h').read_text()
        for name, number in zip(('ChatServer', 'ReqLogin', 'ResLogin', 'ReqSectorMove', 'ResSectorMove',
                                 'ReqMessage', 'ResMessage', 'HeartbeatNoti'), range(8)):
            self.assertIn(f'{name} = {number}', enum)
        self.assertNotIn('ReqHeartbeat', enum)
        login = (self.common / 'Chatting/Login.g.h').read_text()
        self.assertIn('std::array<WCHAR, 20> m_id', login)
        self.assertIn('std::array<char, 64> m_sessionKey', login)
        self.assertNotIn('#include', login)
        chatting = (self.common / 'Chatting/Chatting.g.h').read_text()
        self.assertIn('std::wstring m_message', chatting)
        self.assertRegex(chatting, r'buffer\s*<< m_accountNo\s*<< m_message;')
        self.assertRegex(chatting, r'buffer\s*>> m_accountNo\s*>> m_message;')
        self.assertIn('MHLib::serialization::ISerializable<false>', login)
        self.assertRegex(login, r'buffer\s*<< m_accountNo\s*<< m_id\s*<< m_nickname\s*<< m_sessionKey')
        self.assertIn('Chatting/ChattingPacketAll.g.h', (self.common / 'IncludePacketAll.g.h').read_text())
        self.assertNotIn('PacketTypes.g.h', (self.common / 'Chatting/ChattingPacketAll.g.h').read_text())

    def test_all_fixed_width_integer_types_are_rendered(self):
        types = ('int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64')
        fields = ''.join(f'      - {{name: value_{suffix}, type: {kind}}}\n'
                         for suffix, kind in zip('abcdefgh', types))
        self.source.write_text(NOTI.split('    fields:\n')[0] + '    fields:\n' + fields, encoding='utf-8')
        self.assert_generated(self.generate())
        header = (self.common / 'System/Pulse.g.h').read_text()
        for suffix, kind in zip('ABCDEFGH', types):
            name = f'm_value{suffix}'
            self.assertIn(f'std::{kind}_t {name}', header)
        for operator in ('<<', '>>'):
            chain = r'\s*'.join(f'{operator} m_value{suffix}' for suffix in 'ABCDEFGH')
            self.assertRegex(header, rf'buffer\s*{chain};')
        self.assertEqual(header.count('return buffer.IsValid();'), 2)

    def test_direct_builder_and_dispatch(self):
        self.assert_generated(self.generate(schema=ROOT / 'Common/Packet/Schema'))
        for side, root, packet in (('Client', self.client, 'ReqLogin'), ('Server', self.server, 'ResLogin')):
            with self.subTest(side=side):
                send = (root / f'{side}PacketSend.g.cpp').read_text()
                dispatch = (root / f'{side}PacketDispatch.g.cpp').read_text()
                self.assertIn(f'*pBuffer << static_cast<std::uint16_t>(EPacketType::{packet}) << packet;', send)
                self.assertRegex(send, r'if \(!pBuffer->IsValid\(\)\)\s*\{\s*.*::Free\(pBuffer\);')
                self.assertEqual(dispatch.count('message >> type;'), int(side == 'Client'))
                self.assertRegex(dispatch, r'message >> packet;\s*return message.IsValid\(\)'
                                 r' && message.GetDataSize\(\) == 0\s*&& handler.Handle')
                for obsolete in ('PacketCodec', 'PacketBufferAdapter', 'CPacketBufferWriter', 'CPacketBufferReader'):
                    self.assertNotIn(obsolete, send + dispatch)

    def test_validation_preserves_all_outputs(self):
        self.assert_generated(self.generate())
        baseline = self.snapshot()
        invalid = {
            'duplicate key': NOTI.replace('id: 7', 'id: 7\n    id: 8'),
            'direction is required': NOTI.replace('    direction: client_to_server\n', ''),
            'direction': NOTI.replace('client_to_server', 'both'),
            'req and res': NOTI.replace('    direction: client_to_server\n', '').replace('kind: noti', 'kind: req'),
            'unknown key': NOTI.replace('id: 7', 'id: 7\n    policy: 256'),
            'unsupported type': NOTI.replace('int64', 'float'),
            'positive integer': NOTI.replace('type: int64', 'type: utf16_fixed, code_units: true'),
            'packet id': NOTI.replace('id: 7', 'id: true'),
            'YAML anchors': NOTI.replace('packets:', 'packets: &unused'),
            'YAML anchors ': NOTI.replace('    fields:', '    fields: *unused\n    #'),
            'YAML anchors  ': NOTI.replace('int64', '!!str int64'),
            'duplicate field': NOTI + '      - {name: account_no, type: int8}\n',
            'invalid packet name': NOTI.replace('PulseNoti', 'class'),
            'invalid field name': NOTI.replace('account_no', 'account__no'),
            'duplicate kind': NOTI + NOTI[NOTI.index('  - name'):].replace('PulseNoti', 'OtherNoti').replace('id: 7', 'id: 8'),
        }
        for diagnostic, source in invalid.items():
            with self.subTest(diagnostic=diagnostic):
                self.source.write_text(source, encoding='utf-8')
                result = self.generate()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(diagnostic.strip(), result.stderr)
                self.assertEqual(baseline, self.snapshot())

    def test_duplicates_and_windows_output_name_collisions(self):
        for filename, text, diagnostic in (
            ('Extra.yaml', NOTI.replace('PulseNoti', 'OtherNoti'), 'duplicate packet id'),
            ('Extra.yaml', NOTI.replace('id: 7', 'id: 8'), 'duplicate packet name'),
            ('SystemPacketAll.yaml', NOTI, 'output name collision'),
            ('COM1.yaml', NOTI, 'invalid schema filename'),
            ('Not-Safe.yaml', NOTI, 'invalid schema filename'),
        ):
            with self.subTest(filename=filename, diagnostic=diagnostic):
                other = self.source.with_name(filename)
                other.write_text(text, encoding='utf-8')
                result = self.generate()
                other.unlink()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(diagnostic, result.stderr)
                self.assertFalse(self.common.exists())

    def test_schema_removal_deletes_only_manifest_owned_output(self):
        self.assert_generated(self.generate())
        stale = self.common / 'System/Pulse.g.h'
        manual = stale.with_name('Manual.g.h')
        manual.write_text('handwritten')
        self.source.rename(self.source.with_name('Renamed.yaml'))
        self.assert_generated(self.generate())
        self.assertFalse(stale.exists())
        self.assertEqual(manual.read_text(), 'handwritten')
        (self.common / 'System/Renamed.g.h').unlink()
        self.assert_generated(self.generate())
        self.assertTrue((self.common / 'System/Renamed.g.h').is_file())

    def test_case_only_schema_rename_replaces_manifest_entry(self):
        self.assert_generated(self.generate())
        interim = self.source.with_name('Interim.yaml')
        self.source.rename(interim)
        interim.rename(self.source.with_name('pulse.yaml'))
        self.assert_generated(self.generate())
        self.assertEqual(sorted(p.name for p in (self.common / 'System').glob('*.g.h')),
                         ['SystemPacketAll.g.h', 'pulse.g.h'])

    def test_refuses_output_overlap_and_unmanaged_target(self):
        result = self.generate(client=self.common / 'nested')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('overlap', result.stderr)
        self.assertFalse(self.common.exists())
        target = self.client / 'ClientPacketSend.g.h'
        target.parent.mkdir()
        target.write_text('handwritten')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('unmanaged', result.stderr)
        self.assertFalse(self.common.exists())
        self.assertEqual('handwritten', target.read_text())

    def test_tampered_manifest_cannot_delete_manual_or_escape_root(self):
        self.assert_generated(self.generate())
        manifest, = self.common.glob('*.json')
        outside = self.root / 'Outside.g.h'
        outside.write_text('outside')
        data = json.loads(manifest.read_text())
        data['files']['../Outside.g.h'] = '0' * 64
        manifest.write_text(json.dumps(data))
        before = self.snapshot()
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('manifest', result.stderr)
        self.assertEqual(before, self.snapshot())
        self.assertEqual('outside', outside.read_text())

    def test_managed_file_replaced_with_handwritten_content_is_preserved(self):
        self.assert_generated(self.generate())
        replaced = self.common / 'System/Pulse.g.h'
        replaced.write_text('handwritten')
        self.source.rename(self.source.with_name('Renamed.yaml'))
        before = self.snapshot()
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('modified generated output', result.stderr)
        self.assertEqual(before, self.snapshot())

    def test_git_crlf_checkout_does_not_invalidate_manifest(self):
        self.assert_generated(self.generate())
        output = self.common / 'System/Pulse.g.h'
        output.write_bytes(output.read_bytes().replace(b'\n', b'\r\n'))
        before = self.snapshot()
        self.assert_generated(self.generate())
        self.assertEqual(before, self.snapshot())


if __name__ == '__main__':
    unittest.main()
