# -*- coding: utf-8 -*-
"""
Обёртка для Tomograph4: встроенный Python, похоже, исполняет только файлы
вида Scripts/analytics/<имя>.py, но не вложенные каталоги (autodenoise/denoise.py
получает путь в логе, но код с диска не выполняется).

Реальный сценарий - analytics/denoise_volumeop.py|... -> exec содержимого autodenoise/denoise.py
в том же namespace, что и filtration.py (глобальные sliceNumber, filterParams).
"""
import os
import sys

def _wrapper_dir():
    try:
        return os.path.dirname(os.path.abspath(__file__))
    except Exception:
        pass
    if len(sys.argv) > 0 and sys.argv[0]:
        p = sys.argv[0].split('|', 1)[0]
        if p.lower().endswith('.py'):
            return os.path.dirname(os.path.abspath(p))
    return r'C:\t4vis\Scripts\analytics'


_WRAPPER_DIR = _wrapper_dir()

try:
    with open(
        os.path.join(_WRAPPER_DIR, 'WRAPPER_RAN.txt'),
        'w',
        encoding='utf-8',
    ) as _wf:
        _wf.write('WRAPPER_RAN argv=%r\n' % (sys.argv,))
except Exception:
    pass

_TARGET = os.path.join(_WRAPPER_DIR, 'autodenoise', 'denoise.py')
if not os.path.isfile(_TARGET):
    sys.stderr.write('denoise_volumeop: not found: %s\n' % _TARGET)
    raise SystemExit(1)

with open(_TARGET, encoding='utf-8') as _f:
    _SRC = _f.read()

_NS = globals().copy()
_NS['__file__'] = _TARGET
_NS['__name__'] = '__main__'
exec(compile(_SRC, _TARGET, 'exec'), _NS)
