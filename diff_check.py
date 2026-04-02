import difflib
a = open(r'\\192.168.24.31\Public data\uploads\20260326_v11_newUIv1308\13084_版次連線oK\index.html', encoding='utf-8').read()
b = open(r'\\192.168.24.31\Public data\uploads\20260326_v11_newUIv1308\index.html', encoding='utf-8').read()
d = list(difflib.unified_diff(a.splitlines(), b.splitlines(), lineterm='', n=2))
for l in d[:200]:
    print(l)
