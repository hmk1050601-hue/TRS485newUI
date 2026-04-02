import difflib

for fname in ['sw.js', 'version.js']:
    a = open(r'\\192.168.24.31\Public data\uploads\20260326_v11_newUIv1308\13084_版次連線oK\\' + fname, encoding='utf-8').read()
    b = open(r'\\192.168.24.31\Public data\uploads\20260326_v11_newUIv1308\\' + fname, encoding='utf-8').read()
    d = list(difflib.unified_diff(a.splitlines(), b.splitlines(), lineterm='', n=2))
    if d:
        print(f'=== {fname} ===')
        for l in d:
            print(l)
    else:
        print(f'{fname}: 無差異')
