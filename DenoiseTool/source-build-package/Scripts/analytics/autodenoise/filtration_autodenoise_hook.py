# -*- coding: utf-8 -*-
"""
Опциональный fallback-хук для Tomograph4.

Подключается одной строкой в начале Scripts/analytics/filtration.py и
перехватывает вызовы с filterParams вида denoise:... .
В этом режиме делегирует выполнение в autodenoise/denoise.py.
"""
import os
import sys
import traceback


def _write(path, msg):
    try:
        d = os.path.dirname(path)
        if d:
            os.makedirs(d, exist_ok=True)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(msg)
    except Exception:
        pass


def _analytics_dir(g):
    if '__file__' in g and g['__file__']:
        return os.path.dirname(os.path.abspath(g['__file__']))
    if len(sys.argv) > 0 and sys.argv[0]:
        p = sys.argv[0].split('|', 1)[0]
        if p.lower().endswith('.py'):
            return os.path.dirname(os.path.abspath(p))
    return r'C:\t4vis\Scripts\analytics'


_g = globals()
_fp = _g.get('filterParams', '')
if not (isinstance(_fp, str) and _fp):
    try:
        _a0 = sys.argv[0] if len(sys.argv) > 0 else ''
        _pl = _a0.split('|', 1)[1] if '|' in _a0 else (sys.argv[1] if len(sys.argv) > 1 else '')
        for _it in str(_pl).split(','):
            if '=' not in _it:
                continue
            _k, _v = _it.split('=', 1)
            if _k.strip().strip("'\"") == 'filterParams':
                _fp = _v.strip().strip("'\"")
                break
    except Exception:
        pass

if isinstance(_fp, str) and _fp.startswith('denoise:'):
    try:
        _ana = _analytics_dir(_g)
        _write(
            os.path.join(_ana, 'autodenoise', 'HOOK_RAN.txt'),
            'HOOK_RAN filterParams=%r argv=%r __file__=%r\n'
            % (_fp, sys.argv, _g.get('__file__')),
        )
        _target = os.path.join(_ana, 'autodenoise', 'denoise.py')
        if os.path.isfile(_target):
            _ns = _g.copy()
            _ns['__file__'] = _target
            _ns['__name__'] = '__main__'
            with open(_target, encoding='utf-8') as _f:
                exec(compile(_f.read(), _target, 'exec'), _ns)
            raise SystemExit(0)
        _write(os.path.join(_ana, 'autodenoise', 'HOOK_ERR.txt'), 'target not found: %s\n' % _target)
    except SystemExit:
        raise
    except Exception:
        _write(
            os.path.join(_analytics_dir(_g), 'autodenoise', 'HOOK_ERR.txt'),
            traceback.format_exc(),
        )
        raise
