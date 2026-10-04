"""Union emitted private-header functions without duplicating their denominators.

A header can emit different subsets in different translation units. Each emitted
function must still have the same source and control-flow shape. The final report
requires every function in the declared header inventory to have been emitted.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any

from gcov_coverage import counter, source_hashes


class HeaderFunctionUnion:
    def __init__(self, root: Path, inventory: dict[Path, list[str]]):
        self.root = root.resolve()
        self.inventory = {str(path.resolve()): set(names) for path, names in inventory.items()}
        if any(not names or len(names) != len(set(names)) for names in inventory.values()):
            raise RuntimeError('header inventory requires nonempty unique function names')
        self.hashes = source_hashes(list(inventory))
        self.files: dict[str, dict[str, dict]] = {}
        self.metadata: tuple[str, str] | None = None
        self.profiles: list[dict] = []

    def _verify_sources(self) -> None:
        if source_hashes([Path(path) for path in self.hashes]) != self.hashes:
            raise RuntimeError('private header source changed during coverage measurement')

    def add(self, label: str, document: dict[str, Any]) -> None:
        if label in {profile['name'] for profile in self.profiles}:
            raise RuntimeError('duplicate header coverage profile label: ' + label)
        self._verify_sources()
        metadata = (str(document['format_version']), document['gcc_version'])
        if metadata[0] not in {'1', '2'} or not isinstance(metadata[1], str) or not metadata[1]:
            raise RuntimeError('unsupported header gcov metadata')
        if self.metadata is not None and self.metadata != metadata:
            raise RuntimeError('header coverage profiles use different GCC versions or JSON formats')
        cwd = Path(document['current_working_directory'])
        incoming = {}
        for file in document['files']:
            source = Path(file['file'])
            source = (source if source.is_absolute() else cwd / source).resolve()
            if str(source) not in self.inventory:
                continue
            path = source.relative_to(self.root).as_posix()
            if path in incoming:
                raise RuntimeError('duplicate private header in gcov profile: ' + path)
            functions = {}
            for function in file['functions']:
                name = function['name']
                if name not in self.inventory[str(source)] or name in functions:
                    raise RuntimeError(f'unknown or duplicate header function: {path}:{name}')
                bounds = tuple(counter(function[key]) for key in
                               ('start_line', 'start_column', 'end_line', 'end_column', 'blocks'))
                if bounds[0] <= 0 or bounds[2] < bounds[0] or bounds[4] <= 0:
                    raise RuntimeError(f'invalid header function bounds: {path}:{name}')
                functions[name] = {'bounds': bounds, 'lines': {}, 'branches': {}, 'shape': []}
            seen_lines = set()
            for line in file['lines']:
                number, name = line['line_number'], line.get('function_name')
                if (type(number) is not int or number <= 0 or number in seen_lines or
                        name not in functions):
                    raise RuntimeError(f'unknown or duplicate header line: {path}:{number}')
                seen_lines.add(number)
                function = functions[name]
                if not function['bounds'][0] <= number <= function['bounds'][2]:
                    raise RuntimeError(f'header line outside function bounds: {path}:{number}')
                function['lines'][number] = counter(line['count'])
                blocks = tuple(counter(block) for block in line.get('block_ids', []))
                outcomes = []
                for index, branch in enumerate(line['branches']):
                    edge = (counter(branch['source_block_id']), counter(branch['destination_block_id'])) \
                        if metadata[0] == '2' else (index,)
                    flags = (branch['fallthrough'], branch['throw'])
                    if any(type(flag) is not bool for flag in flags):
                        raise RuntimeError(f'invalid header branch flags: {path}:{number}')
                    key = (number, *edge)
                    if key in function['branches']:
                        raise RuntimeError(f'duplicate header branch identity: {path}:{name}:{key}')
                    function['branches'][key] = counter(branch['count'])
                    outcomes.append((edge, flags))
                function['shape'].append((number, blocks, tuple(outcomes)))
            for name, function in functions.items():
                if not function['lines']:
                    raise RuntimeError(f'header function has no counter lines: {path}:{name}')
                function['signature'] = (function['bounds'], tuple(sorted(function.pop('shape'))))
                previous = self.files.get(path, {}).get(name)
                if previous and previous['signature'] != function['signature']:
                    raise RuntimeError(f'incompatible header function control-flow profiles: {path}:{name}')
                # A physical header line must not be assigned to distinct functions.
                for other_name, other in self.files.get(path, {}).items():
                    if other_name != name and function['lines'].keys() & other['lines'].keys():
                        raise RuntimeError(f'overlapping private header function lines: {path}:{name}')
            incoming[path] = functions
        # Commit only after the entire document has passed validation.
        contribution = {'name': label, 'new_lines': 0, 'new_branches': 0,
                        'compiled_functions': {path: sorted(functions) for path, functions in sorted(incoming.items())}}
        for path, functions in incoming.items():
            stored = self.files.setdefault(path, {})
            for name, function in functions.items():
                if name not in stored:
                    stored[name] = {'signature': function['signature'],
                                    'lines': dict.fromkeys(function['lines'], 0),
                                    'branches': dict.fromkeys(function['branches'], 0)}
                for kind in ('lines', 'branches'):
                    for key, count in function[kind].items():
                        contribution['new_' + kind] += int(count > 0 and stored[name][kind][key] == 0)
                        stored[name][kind][key] += count
        self.metadata = metadata
        self.profiles.append(contribution)

    def report(self) -> dict[str, Any]:
        self._verify_sources()
        if not self.profiles:
            raise RuntimeError('cannot report header coverage without executed profiles')
        files = {}
        total = {'functions': 0, 'lines': 0, 'covered_lines': 0, 'branches': 0, 'covered_branches': 0}
        for absolute, names in sorted(self.inventory.items()):
            path = Path(absolute).relative_to(self.root).as_posix()
            functions = self.files.get(path, {})
            if functions.keys() != names:
                raise RuntimeError(f'private header functions not emitted: {path}:{sorted(names - functions.keys())}')
            details = {}
            for name, function in sorted(functions.items()):
                summary = {kind: len(function[kind]) for kind in ('lines', 'branches')}
                summary.update({'covered_' + kind: sum(count > 0 for count in function[kind].values())
                                for kind in ('lines', 'branches')})
                summary['uncovered_lines'] = sorted(line for line, count in function['lines'].items() if count == 0)
                summary['uncovered_branches'] = [list(key) for key, count in sorted(function['branches'].items()) if count == 0]
                details[name] = summary
            summary = {'functions': len(details),
                       **{key: sum(detail[key] for detail in details.values()) for key in total if key != 'functions'}}
            for key in total:
                total[key] += summary[key]
            files[path] = {**summary, 'function_details': details}
        for summary in [*files.values(), total]:
            for kind in ('lines', 'branches'):
                summary[kind + '_percent'] = 100.0 * summary['covered_' + kind] / summary[kind] if summary[kind] else 100.0
        return {'gcc_version': self.metadata[1], 'gcov_format': self.metadata[0],
                'source_sha256': {Path(path).relative_to(self.root).as_posix(): digest for path, digest in self.hashes.items()},
                'profiles': self.profiles, 'files': files, 'total': total}
