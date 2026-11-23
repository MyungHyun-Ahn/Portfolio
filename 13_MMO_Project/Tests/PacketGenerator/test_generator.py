import hashlib
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

    def generate(self, *, schema=None, client=None, extra=()):
        return subprocess.run([
            sys.executable, str(CLI), '--schema-dir', str(schema or self.schema),
            '--common-out', str(self.common), '--client-out', str(client or self.client),
            '--server-out', str(self.server), *extra,
        ], cwd=self.root, capture_output=True, text=True, encoding='utf-8')

    def assert_generated(self, result):
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_client_members_created_only_when_missing(self):
        members = self.root / 'manual' / 'CChatNetClientMembers.inc'
        options = ('--client-members-out', str(members))
        self.assert_generated(self.generate(extra=options))
        self.assertRegex(members.read_bytes(), rb'bool ReqLogin\([^;]+&sessionKey\) noexcept;')
        self.assertIn(b'bool ReqSectorMove(', members.read_bytes())
        self.assertNotIn(b'm_initialSector', members.read_bytes())
        self.assertNotIn(b'enum class ELoginState', members.read_bytes())
        self.assertNotIn(b'struct SSectorPosition', members.read_bytes())
        self.assertNotIn(b'submitChatRequest', members.read_bytes())
        self.assertNotIn(members.name, (self.client / '.packet-generator.json').read_text())

        members.write_bytes(b'// user-owned declaration\r\n')
        before = (members.read_bytes(), members.stat().st_mtime_ns)
        self.assert_generated(self.generate(extra=options))
        self.assertEqual(before, (members.read_bytes(), members.stat().st_mtime_ns))
        members.write_bytes(b'')
        self.assert_generated(self.generate(extra=options))
        self.assertEqual(b'', members.read_bytes())

    def test_invalid_schema_does_not_create_client_members(self):
        members = self.root / 'manual' / 'CChatNetClientMembers.inc'
        self.source.write_text('not a mapping', encoding='utf-8')
        result = self.generate(extra=('--client-members-out', str(members)))
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(members.exists())

    def snapshot(self):
        return {path: (path.read_bytes(), path.stat().st_mtime_ns)
                for root in (self.common, self.client, self.server) for path in root.rglob('*') if path.is_file()}

    def test_single_noti_projects_by_direction_and_identical_run_keeps_mtimes(self):
        self.assert_generated(self.generate())
        self.assertIn('SPulseNoti', (self.common / 'System/Pulse.g.h').read_text())
        self.assertIn('MakePacketPulseNoti', (self.client / 'CChatNetClient.g.h').read_text(encoding='utf-8'))
        self.assertNotIn('HandlePulseNoti', (self.client / 'IClientPacketHandler.g.h').read_text())
        self.assertIn('HandlePulseNoti', (self.server / 'IServerPacketHandler.g.h').read_text())
        self.assertNotIn('MakePacketPulseNoti', (self.server / 'ServerPacketSend.g.h').read_text(encoding='utf-8'))
        before = self.snapshot()
        self.assert_generated(self.generate())
        self.assertEqual(before, self.snapshot())

    def test_production_packet_contract(self):
        self.assert_generated(self.generate(schema=ROOT / 'Common/Packet/Schema'))
        enum = (self.common / 'PacketTypes.g.h').read_text()
        for name, number in zip(('ChatServer', 'ReqLogin', 'ResLogin', 'ReqSectorMove', 'ResSectorMove',
                                 'ReqMessage', 'ResMessage', 'HeartbeatNoti', 'MessageNoti'), range(9)):
            self.assertRegex(enum, rf'\b{name} = {number},')
        self.assertNotIn('ReqHeartbeat', enum)
        login = (self.common / 'Chatting/Login.g.h').read_text()
        self.assertIn('std::array<char, 64> m_sessionKey', login)
        self.assertNotIn('#include', login)
        chatting = (self.common / 'Chatting/Chatting.g.h').read_text()
        self.assertIn('std::wstring m_message', chatting)
        for header in (login, chatting):
            self.assertIn('std::wstring m_id', header)
            self.assertIn('std::wstring m_nickname', header)
        self.assertRegex(chatting, r'buffer\s*<< m_accountNo\s*<< m_message;')
        self.assertRegex(chatting, r'buffer\s*>> m_accountNo\s*>> m_message;')
        for name in ('ResMessage', 'MessageNoti'):
            dto = chatting.split(f'struct S{name}', 1)[1].split('\n\t};', 1)[0]
            self.assertRegex(dto, r'buffer\s*<< m_accountNo\s*<< m_id\s*<< m_nickname\s*<< m_message;')
            self.assertRegex(dto, r'buffer\s*>> m_accountNo\s*>> m_id\s*>> m_nickname\s*>> m_message;')
            self.assertIn(f'Handle{name}', (self.client / 'ClientPacketDispatch.g.cpp').read_text(encoding='utf-8'))
            self.assertNotIn(f'Handle{name}', (self.server / 'ServerPacketDispatch.g.cpp').read_text(encoding='utf-8'))
        self.assertIn('MakePacketMessageNoti', (self.server / 'ServerPacketSend.g.h').read_text(encoding='utf-8'))
        self.assertIn('MHLib::serialization::ISerializable', login)
        self.assertRegex(login, r'buffer\s*<< m_accountNo\s*<< m_id\s*<< m_nickname\s*<< m_sessionKey')
        self.assertIn('Chatting/ChattingPacketAll.g.h', (self.common / 'IncludePacketAll.g.h').read_text())
        self.assertNotIn('PacketTypes.g.h', (self.common / 'Chatting/ChattingPacketAll.g.h').read_text())

    def test_client_sender_migrates_manifest_without_rewriting_manual_or_server_files(self):
        self.client.mkdir()
        files = {}
        for suffix in ('h', 'cpp'):
            old_name = f'ClientPacketSend.g.{suffix}'
            data = b'// PacketGenerator generated file. Do not edit.\n'
            (self.client / old_name).write_bytes(data)
            files[old_name] = hashlib.sha256(data).hexdigest()
        manifest = self.client / '.packet-generator.json'
        manifest.write_text(json.dumps(dict(version=1, role='Client', files=files)), encoding='utf-8')
        manual = self.root / 'CChatNetClientMembers.inc'
        manual.write_text('// handwritten', encoding='utf-8')
        manual_before = (manual.read_bytes(), manual.stat().st_mtime_ns)

        production_schema = ROOT / 'Common/Packet/Schema'
        self.assert_generated(self.generate(schema=production_schema))
        current = json.loads(manifest.read_text(encoding='utf-8'))['files']
        for suffix in ('h', 'cpp'):
            self.assertFalse((self.client / f'ClientPacketSend.g.{suffix}').exists())
            self.assertIn(f'CChatNetClient.g.{suffix}', current)
        header = (self.client / 'CChatNetClient.g.h').read_text(encoding='utf-8')
        self.assertIn('#include "CChatNetClientMembers.inc"', header)
        for response in ('Login', 'SectorMove', 'Message'):
            self.assertIn(f'bool HandleRes{response}(const SRes{response} &packet) noexcept override final;', header)
        production_server = ROOT / 'Tests/Chatting/Server/Generated/PacketHandler'
        for path in production_server.iterdir():
            self.assertEqual(path.read_bytes().replace(b'\r\n', b'\n'), (self.server / path.name).read_bytes())
        before = self.snapshot()
        self.assert_generated(self.generate(schema=production_schema))
        self.assertEqual(before, self.snapshot())
        self.assertEqual(manual_before, (manual.read_bytes(), manual.stat().st_mtime_ns))

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
                send_stem = 'CChatNetClient' if side == 'Client' else 'ServerPacketSend'
                send = (root / f'{send_stem}.g.cpp').read_text(encoding='utf-8')
                dispatch = (root / f'{side}PacketDispatch.g.cpp').read_text()
                header = (root / f'{send_stem}.g.h').read_text(encoding='utf-8')
                if side == 'Client':
                    self.assertIn('std::shared_ptr<NetworkCommonLib::CSerializableBuffer>', header)
                    self.assertIn('CSerializableBuffer::MakeShared()', send)
                    self.assertIn(f'*spBuffer << static_cast<std::uint16_t>(EPacketType::{packet}) << packet;', send)
                    self.assertRegex(send, r'if \(!spBuffer->IsValid\(\)\)\s*\{\s*return nullptr;')
                    self.assertNotIn('::Free(', send)
                else:
                    self.assertIn('NetworkCommonLib::CSerializableBuffer *MakePacket', header)
                    self.assertIn('CSerializableBuffer::Alloc()', send)
                    self.assertIn(f'*pBuffer << static_cast<std::uint16_t>(EPacketType::{packet}) << packet;', send)
                    self.assertRegex(send, r'if \(!pBuffer->IsValid\(\)\)\s*\{\s*.*::Free\(pBuffer\);')
                self.assertEqual(dispatch.count('message >> type;'), int(side == 'Client'))
                self.assertRegex(dispatch, r'message >> packet;\s*return message.IsValid\(\)'
                                 r' && message.GetDataSize\(\) == 0\s*&& handler.Handle')
                for obsolete in ('PacketCodec', 'PacketBufferAdapter', 'CPacketBufferWriter', 'CPacketBufferReader'):
                    self.assertNotIn(obsolete, send + dispatch)

    def test_generated_senders_use_field_arguments_and_distinct_ownership(self):
        self.assert_generated(self.generate(schema=ROOT / 'Common/Packet/Schema'))
        client = (self.client / 'CChatNetClient.g.h').read_text(encoding='utf-8')
        server = (self.server / 'ServerPacketSend.g.h').read_text(encoding='utf-8')
        client_cpp = (self.client / 'CChatNetClient.g.cpp').read_text(encoding='utf-8')
        server_cpp = (self.server / 'ServerPacketSend.g.cpp').read_text()
        self.assertIn('class CChatNetClient : public ClientNetworkLib::CClient, private IClientPacketHandler', client)
        self.assertRegex(client, r'private:\s*bool onRecv\(NetworkCommonLib::CSerializableBuffer &message\) noexcept override final;')
        self.assertRegex(client_cpp, r'bool CChatNetClient::onRecv\(NetworkCommonLib::CSerializableBuffer &message\) noexcept\s*'
                                     r'\{\s*return DispatchClientPacket\(message, \*this\);\s*\}')
        self.assertIn('bool CChatNetClient::SendReqMessage(', client_cpp)
        self.assertNotIn('CClientPacketSender', client + client_cpp)
        self.assertNotIn('virtual bool sendPacket(', client)
        self.assertIn('class CServerPacketSender', server)
        self.assertRegex(client, r'bool SendReqMessage\(\s*std::int64_t accountNo,\s*const std::wstring &message\) noexcept;')
        self.assertRegex(client, r'bool SendNotiHeartbeat\(\s*std::int64_t accountNo\) noexcept;')
        self.assertRegex(client, r'const std::wstring &id,\s*const std::wstring &nickname,\s*'
                                 r'const std::array<char, 64> &sessionKey')
        self.assertRegex(server, r'SendResMessage\([\s\S]*?const std::wstring &id,\s*const std::wstring &nickname,\s*'
                                 r'const std::wstring &message')
        for name in ('Login', 'SectorMove', 'Message'):
            self.assertRegex(server, rf'\*SendRes{name}\(\s*UINT64 sessionId,')
            self.assertNotIn(f'SendRes{name}', client)
        self.assertNotRegex(server, r'SendReq|SendNotiHeartbeat')
        self.assertRegex(server, r'\*SendNotiMessage\(\s*UINT64 sessionId,')
        self.assertNotIn('SendNotiMessage', client)
        self.assertIn('packet.m_message = message;', client_cpp)
        self.assertRegex(client_cpp, r'catch \(const std::bad_alloc &\)\s*\{\s*return false;')
        for name in ('ReqLogin', 'ReqSectorMove', 'ReqMessage', 'HeartbeatNoti'):
            self.assertIn(f'return this->sendPacket(MakePacket{name}(packet));', client_cpp)
        self.assertNotIn('submitChatRequest', client_cpp)
        self.assertRegex(server_cpp, r'catch \(const std::bad_alloc &\)\s*\{\s*return nullptr;')
        self.assertRegex(server_cpp, r'submitPreparedPacket\(sessionId, EPacketType::ResMessage,\s*MakePacketResMessage\(packet\)\)')
        self.assertRegex(server_cpp, r'if \(pPacket == nullptr\)\s*\{\s*return nullptr;\s*\}\s*'
                                     r'pPacket->IncreaseRef\(\);\s*sendPacket\(sessionId, type, \*pPacket\);\s*return pPacket;')

    def test_server_noti_and_field_parameter_name_conflicts(self):
        schema = NOTI.replace('client_to_server', 'server_to_client').replace('account_no', 'session_id')
        schema += ('      - {name: packet, type: uint8}\n'
                   '      - {name: class, type: uint8}\n'
                   '      - {name: packet_value, type: uint8}\n')
        self.source.write_text(schema, encoding='utf-8')
        empty = schema[:schema.index('    fields:')].replace('PulseNoti', 'NotiReady').replace('id: 7', 'id: 8')
        self.source.with_name('Ready.yaml').write_text(empty + '    fields: []\n', encoding='utf-8')
        self.assert_generated(self.generate())
        server = (self.server / 'ServerPacketSend.g.h').read_text(encoding='utf-8')
        server_cpp = (self.server / 'ServerPacketSend.g.cpp').read_text()
        self.assertRegex(server, r'\*SendNotiPulse\(\s*UINT64 sessionId,\s*std::int64_t sessionIdValue,\s*'
                                 r'std::uint8_t packetValue2,\s*std::uint8_t classValue,\s*std::uint8_t packetValue\) noexcept;')
        self.assertRegex(server, r'\*SendNotiReady\(\s*UINT64 sessionId\) noexcept;')
        self.assertIn('packet.m_sessionId = sessionIdValue;', server_cpp)
        self.assertIn('packet.m_packet = packetValue2;', server_cpp)
        self.assertNotIn('SendNotiPulse', (self.client / 'CChatNetClient.g.h').read_text(encoding='utf-8'))
        self.assertIn('HandlePulseNoti', (self.client / 'IClientPacketHandler.g.h').read_text())

    def test_generated_send_name_collision_is_rejected_before_writing(self):
        self.assert_generated(self.generate())
        before = self.snapshot()
        other = NOTI.replace('PulseNoti', 'NotiPulse').replace('id: 7', 'id: 8')
        self.source.with_name('Other.yaml').write_text(other, encoding='utf-8')
        result = self.generate()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('duplicate sender name: SendNotiPulse', result.stderr)
        self.assertEqual(before, self.snapshot())

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
        target = self.client / 'CChatNetClient.g.h'
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
