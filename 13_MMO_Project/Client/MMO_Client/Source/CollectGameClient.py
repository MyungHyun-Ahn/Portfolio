"""Collect Win64 Development build artifacts from a completed UBT build (no cook)."""
import argparse
import json
import shutil
from pathlib import Path


def collect(engine):
    project = Path(__file__).resolve().parents[1]
    binaries = project / 'Binaries/Win64'
    output = project.parents[1] / 'Build/Client/Development/GameClient'
    receipt = binaries / 'GameClient.target'
    data = json.loads(receipt.read_text(encoding='utf-8-sig'))
    prefix = '$(ProjectDir)/Binaries/Win64/'
    identity = ('GameClient', 'Game', 'Win64', 'x64', 'Development', prefix + 'GameClient.exe')
    if tuple(data[k] for k in ('TargetName', 'TargetType', 'Platform', 'Architecture', 'Configuration', 'Launch')) != identity:
        raise ValueError(f'Unexpected GameClient receipt: {receipt}')

    files = {Path(receipt.name): receipt}
    roots = [(prefix, binaries, Path('.')), ('$(EngineDir)/', engine, Path('Engine')),
             ('$(ProjectDir)/', project, Path('Project'))]
    entries = [e for e in data['BuildProducts'] if e['Type'] not in ('StaticLibrary', 'ImportLibrary')]
    entries += [e for e in data['RuntimeDependencies'] if e['Type'] == 'NonUFS']
    for entry in entries:
        for token, source, destination in roots:
            if entry['Path'].startswith(token):
                relative = Path(entry['Path'][len(token):])
                if relative.anchor or '..' in relative.parts:
                    raise ValueError(entry['Path'])
                key = destination / relative
                if files.setdefault(key, source / relative) != source / relative:
                    raise ValueError(f'Conflicting receipt entries: {key}')
                break
        else:
            raise ValueError(f"Unknown receipt path: {entry['Path']}")

    if Path('GameClient.exe') not in files:
        raise ValueError('Launch executable missing from receipt products')
    for source in files.values():
        if not source.is_file():
            raise FileNotFoundError(source)
    unexpected = [f for f in output.rglob('*') if f.is_file() and f.relative_to(output) not in files]
    if unexpected:
        raise ValueError(f'Unexpected output file; move aside before collection: {unexpected[0]}')
    for relative, source in files.items():
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    print(f'Collected {len(files)} GameClient Development build artifacts: {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('engine', type=Path, help='UE Engine directory')
    collect(parser.parse_args().engine)
