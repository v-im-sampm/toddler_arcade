#!/usr/bin/env python3
import json
import sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
c=json.loads((root/'config/commands.json').read_text())
assert c['activation']=='push_to_talk'
ids=[i['id'] for i in c['intents']]; names=[i['name'] for i in c['intents']]
assert len(ids)==len(set(ids)) and len(names)==len(set(names))
assert all(i['aliases'] and i['contexts'] for i in c['intents'])
assert len(list((root/'games').glob('*.md')))==10
sys.path.insert(0, str(Path(__file__).resolve().parent))
from board_pins import check_file
for cfg in ('config/board_pins.json', 'config/board_pins.example.json'):
    errs = check_file(root/cfg)
    assert not errs, f'{cfg}: {errs}'
print('PASS: command IDs, ten game specs, hardware pin safety gate')
