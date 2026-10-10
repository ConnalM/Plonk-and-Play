"""Guard compiled loopTask hot-path frames against result-capacity regressions."""
from pathlib import Path
import argparse, sys

try:
    from elftools.elf.elffile import ELFFile
except Exception as exc:
    print(f"loop stack guard unavailable: {exc}")
    raise SystemExit(2)

parser = argparse.ArgumentParser()
parser.add_argument('--elf', action='append', required=True)
args = parser.parse_args()

targets = {
    '_ZN2pp16RaceEngineModule5resetEy': 2048,
    '_ZN2pp16RaceEngineModule4sealEv': 2048,
    '_Z4loopv': 4096,
}
browser_fragments = {
    'state': 'BrowserInterface5state',
    'results': 'BrowserInterface12resultsRoute',
    'details': 'BrowserInterface12detailsRoute',
    'finish-display': 'BrowserInterface18finishDisplayRoute',
    'display-result': 'BrowserInterface13displayResult',
}
browser_frame_limit = 2048
failed = []

for name in args.elf:
    path = Path(name)
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        symtab = elf.get_section_by_name('.symtab')
        symbols = {s.name: s['st_value'] for s in symtab.iter_symbols()} if symtab else {}
        cfi = list(elf.get_dwarf_info().CFI_entries())
        rows = {}
        by_name = {}
        for symbol in symtab.iter_symbols() if symtab else []:
            if symbol['st_value']:
                by_name[symbol.name] = symbol['st_value']
        for symbol, limit in targets.items():
            address = symbols.get(symbol)
            if address is None:
                failed.append(f'{path}:missing:{symbol}')
                continue
            fdes = [e for e in cfi if e.__class__.__name__ == 'FDE' and
                    e['initial_location'] <= address < e['initial_location'] + e['address_range']]
            if not fdes:
                failed.append(f'{path}:missing-cfi:{symbol}')
                continue
            table = fdes[0].get_decoded().table
            frame = table[1]['cfa'].offset if len(table) > 1 else 0
            rows[symbol] = frame
            if frame > limit:
                failed.append(f'{path}:{symbol}:{frame}>{limit}')
        for label, fragment in browser_fragments.items():
            matches = [name for name in by_name if fragment in name]
            if not matches:
                failed.append(f'{path}:missing-browser:{label}:{fragment}')
                continue
            for name in matches:
                address = by_name[name]
                fdes = [e for e in cfi if e.__class__.__name__ == 'FDE' and
                        e['initial_location'] <= address < e['initial_location'] + e['address_range']]
                if not fdes:
                    failed.append(f'{path}:missing-cfi-browser:{name}')
                    continue
                table = fdes[0].get_decoded().table
                frame = table[1]['cfa'].offset if len(table) > 1 else 0
                rows[f'browser:{label}:{name}'] = frame
                if frame > browser_frame_limit:
                    failed.append(f'{path}:browser:{name}:{frame}>{browser_frame_limit}')
        for name, address in by_name.items():
            if 'VersionedResultCodec' not in name and 'VersionedHistoryStore' not in name:
                continue
            fdes = [e for e in cfi if e.__class__.__name__ == 'FDE' and
                    e['initial_location'] <= address < e['initial_location'] + e['address_range']]
            if not fdes:
                continue
            table = fdes[0].get_decoded().table
            frame = table[1]['cfa'].offset if len(table) > 1 else 0
            rows[f'persist:{name}'] = frame
            if frame > browser_frame_limit:
                failed.append(f'{path}:persist:{name}:{frame}>{browser_frame_limit}')
        print(f'LOOP STACK PASS {path}: ' + ', '.join(f'{k}={v}' for k, v in rows.items()))

if failed:
    print('LOOP STACK FAIL: ' + ', '.join(failed))
    raise SystemExit(1)
