#"""
#Денойзинг томографии SwinIR (VolumeOperationTool.AutoDenoiseModule).

#Важно: инструмент "2D фильтрация" с режимом ИИ использует analytics/filtration.py и
#внутренний WrapFilter - это другой код, не этот файл. Сообщения вида script!...denoise.py
#в консоли часто означают только регистрацию пути модуля; реальное выполнение - кнопки
#"Срез" / "Старт" в панели AutoDenoise (executeInterpreter).

#Формат вызова как у filtration.py: строка после "|" и глобальные sliceNumber, filterParams, filtpath.
#"""
import os
import sys

# Самый ранний маркер: если после "Срез" нет PY_RAN.txt - этот файл не выполнялся (другой путь хоста).
try:
    _boot_msg = 'PY_RAN __name__=%r argv=%r cwd=%r\n' % (
        globals().get('__name__', '?'),
        sys.argv,
        os.getcwd(),
    )
    _boot_paths = [
        r'C:\t4vis\Scripts\analytics\autodenoise\PY_RAN.txt',
        os.path.join(os.environ.get('TEMP', r'C:\Windows\Temp'), 't4_denoise_PY_RAN.txt'),
    ]
    _la = os.environ.get('LOCALAPPDATA')
    if _la:
        _boot_paths.append(os.path.join(_la, 't4_denoise_PY_RAN.txt'))
    _ud = os.environ.get('USERPROFILE', '')
    if _ud:
        _boot_paths.append(os.path.join(_ud, 'Desktop', 't4_denoise_PY_RAN.txt'))
        _boot_paths.append(os.path.join(_ud, 'Documents', 't4_denoise_PY_RAN.txt'))
    for _p in _boot_paths:
        if not _p:
            continue
        try:
            _d = os.path.dirname(os.path.abspath(_p))
            if _d:
                os.makedirs(_d, exist_ok=True)
            with open(_p, 'w', encoding='utf-8', errors='replace') as _bf:
                _bf.write(_boot_msg)
        except Exception:
            pass
except Exception:
    pass

# При запуске через exec из filtration.py Python не добавляет каталог скрипта в sys.path.
# Это ломает импорты вида "from models..." / "from utils...".
try:
    _THIS_DIR = os.path.dirname(os.path.abspath(__file__))
except Exception:
    _THIS_DIR = r'C:\t4vis\Scripts\analytics\autodenoise'
if _THIS_DIR and _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)

_TRACE = os.path.join(os.environ.get('TEMP', r'C:\Windows\Temp'), 't4_denoise_trace.txt')
_FIXED = r'C:\t4vis\t4_denoise_trace.txt'


def _append_trace(line: str) -> None:
    for path in (_TRACE, _FIXED):
        try:
            with open(path, 'a', encoding='utf-8') as f:
                f.write(line)
                if not line.endswith('\n'):
                    f.write('\n')
        except Exception:
            continue


# Любая загрузка файла (import или exec): иначе непонятно, читает ли Python этот .py вообще.
try:
    _nm = __name__
except Exception:
    _nm = '?'
_append_trace('LOAD __name__=%r argv=%r cwd=%r\n' % (_nm, sys.argv, os.getcwd()))


def _host_injected_filtration_globals() -> bool:
#    """Tomograph4 часто подставляет sliceNumber/filterParams в namespace exec, не заполняя sys.argv."""
    g = globals()
    for key in ('sliceNumber', 'filterParams', 'filtpath'):
        if key in g:
            return True
    return False


def _should_run_entry() -> bool:
#    """Реальный сценарий denoise - не "голый" import пакета без контекста тома."""
    n = globals().get('__name__', '')
    if n == '__main__':
        return True
    if _host_injected_filtration_globals():
        return True
    av = list(sys.argv) if sys.argv else []
    if len(av) > 1:
        return True
    if not av:
        return False
    a0 = av[0].replace('\\', '/')
    if 'denoise.py' in a0.lower():
        return True
    if '|' in a0:
        return True
    return False


import traceback
from pathlib import Path
from typing import TYPE_CHECKING, Optional

if TYPE_CHECKING:
    import numpy as np
    import torch
    from torch import nn

_LOG_NAME = 'denoise_last_run.txt'


def _debug_log(msg: str) -> None:
    paths = []
    try:
        paths.append(Path(__file__).resolve().parent / _LOG_NAME)
    except NameError:
        pass
    paths.extend(
        [
            Path(r'C:\t4vis\Scripts\analytics\autodenoise') / _LOG_NAME,
            Path(r'C:\t4vis\tmp') / _LOG_NAME,
            Path(os.environ.get('TEMP', r'C:\Windows\Temp')) / 't4_denoise_last_run.txt',
            Path(os.getcwd()) / _LOG_NAME,
        ]
    )
    for p in paths:
        try:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(msg, encoding='utf-8')
            print(f'[denoise] log -> {p}', flush=True)
            return
        except Exception:
            continue
    try:
        print(msg, file=sys.stderr, flush=True)
    except Exception:
        pass


def parse_overrides(overrides_str: str) -> dict:
    params = {}
    for item in overrides_str.split(','):
        if '=' not in item:
            continue
        key, value = item.split('=', 1)
        key = key.strip().strip("'")
        value = value.strip().strip("'")
        params[key] = value
    return params


def param_line_from_argv() -> str:
    if len(sys.argv) > 1:
        return sys.argv[1]
    if len(sys.argv) > 0 and '|' in sys.argv[0]:
        return sys.argv[0].split('|', 1)[1]
    return ''


def resolve_model_path(model_file: str) -> Path:
    return Path(appDirectory) / 'ai-models' / 'denoise' / model_file
    mf = Path(model_file)
    if mf.is_absolute() and mf.exists():
        return mf
    script_dir = Path(__file__).resolve().parent
    cand = script_dir / 'models' / model_file
    if cand.exists():
        return cand
    cand = Path('ai-models/denoise') / model_file
    if cand.exists():
        return cand.resolve()
    return script_dir / 'models' / model_file


def load_model(model_path: Path, device: str = 'cuda'):
    import torch
    from models.network_swinir import SwinIR

    model = SwinIR(
        img_size=128,
        in_chans=1,
        embed_dim=180,
        depths=[6, 6, 6, 6, 6, 6],
        num_heads=[6, 6, 6, 6, 6, 6],
        window_size=8,
        mlp_ratio=2,
        img_range=1.0,
        upsampler='',
        resi_connection='1conv',
        upscale=1,
    )
    if model_path.exists():
        checkpoint = torch.load(str(model_path), map_location=device)
        if isinstance(checkpoint, dict):
            if 'state_dict' in checkpoint:
                checkpoint = checkpoint['state_dict']
            elif 'model' in checkpoint:
                checkpoint = checkpoint['model']
        model.load_state_dict(checkpoint, strict=False)
        print(f'[Model] Loaded {model_path}')
    else:
        print(f'[Error] Model file not found: {model_path}')
    model.to(device)
    model.eval()
    return model


def _to_model_input(image, cv2_mod, np):
    if image.ndim == 2:
        return image[:, :, np.newaxis]
    if image.ndim == 3 and image.shape[2] >= 3:
        gray = cv2_mod.cvtColor(image, cv2_mod.COLOR_BGR2GRAY)
        return gray[:, :, np.newaxis]
    return image


def denoise_image(model, image, device: str = 'cuda'):
    import cv2
    import numpy as np
    import torch

    image = _to_model_input(image, cv2, np)
    with torch.no_grad():
        img_tensor = torch.from_numpy(image).float() / 255.0
        img_tensor = img_tensor.permute(2, 0, 1).unsqueeze(0).to(device)
        output = tile_process(img_tensor, model, 128, 32, 8)
        output = output.squeeze(0).permute(1, 2, 0).cpu().numpy()
        output = np.clip(output * 255.0, 0, 255).astype(np.uint8)
    if output.ndim == 3 and output.shape[2] == 1:
        output = output[:, :, 0]
    return output


def tile_process(
    img_tensor,
    model,
    tile_size: Optional[int] = None,
    tile_overlap: int = 32,
    window_size: int = 8,
):
    import torch
    from utils.patchify import pad_to_window_size

    if tile_size is None:
        padded, _h_pad, _w_pad = pad_to_window_size(img_tensor, window_size)
        with torch.no_grad():
            output = model(padded)
        _, _, h_old, w_old = img_tensor.size()
        return output[..., :h_old, :w_old]

    b, c, h, w = img_tensor.size()
    tile = min(tile_size, h, w)
    tile = (tile // window_size) * window_size
    stride = tile - tile_overlap
    h_idx_list = list(range(0, h - tile, stride)) + [h - tile]
    w_idx_list = list(range(0, w - tile, stride)) + [w - tile]

    e_acc = torch.zeros(b, c, h, w, dtype=img_tensor.dtype, device=img_tensor.device)
    w_acc = torch.zeros_like(e_acc)

    with torch.no_grad():
        for h_idx in h_idx_list:
            for w_idx in w_idx_list:
                in_patch = img_tensor[..., h_idx : h_idx + tile, w_idx : w_idx + tile]
                padded, _, _ = pad_to_window_size(in_patch, window_size)
                out_patch = model(padded)
                out_patch = out_patch[..., :tile, :tile]
                mask = torch.ones_like(out_patch)
                e_acc[..., h_idx : h_idx + tile, w_idx : w_idx + tile] += out_patch
                w_acc[..., h_idx : h_idx + tile, w_idx : w_idx + tile] += mask

    return e_acc / w_acc


def _get_t4_api():
#    """В режиме executeInterpreter t4 может быть только глобалом, без importable модуля."""
    try:
        import t4 as t4m  # type: ignore
        return t4m
    except Exception:
        pass
    g = globals()
    if 't4' in g:
        return g['t4']
    raise RuntimeError("t4 API is not available (no module and no global 't4').")


def _resolve_device(params: dict) -> str:
    import torch

    requested = str(params.get('device', 'Auto')).strip().lower()
    if requested in ('cuda', 'gpu'):
        return 'cuda' if torch.cuda.is_available() else 'cpu'
    if requested == 'cpu':
        return 'cpu'
    return 'cuda' if torch.cuda.is_available() else 'cpu'


def _cuda_diag(params: dict) -> dict:
    requested = str(params.get('device', 'Auto')).strip()
    info = {
        'requested_device': requested,
        'resolved_device': 'cpu',
        'cuda_available': False,
        'cuda_device_count': 0,
    }
    try:
        import torch

        info['cuda_available'] = bool(torch.cuda.is_available())
        info['cuda_device_count'] = int(torch.cuda.device_count())
        info['resolved_device'] = _resolve_device(params)
        if info['resolved_device'] == 'cuda' and info['cuda_device_count'] > 0:
            try:
                info['cuda_device_name'] = torch.cuda.get_device_name(0)
            except Exception:
                pass
    except Exception:
        pass
    return info


def _progress_path(params: dict) -> str:
    p = str(params.get('progress_path', '')).strip()
    if p:
        return p
    return os.path.join(os.environ.get('TEMP', r'C:\Windows\Temp'), 't4_denoise_progress.txt')


def _write_progress(params: dict, percent: int) -> None:
    p = _progress_path(params)
    try:
        d = os.path.dirname(os.path.abspath(p))
        if d:
            os.makedirs(d, exist_ok=True)
        with open(p, 'w', encoding='utf-8') as f:
            f.write(str(max(0, min(100, int(percent)))))
    except Exception:
        pass


def process_slice(params: dict, slice_number: int) -> None:
    import cv2
    import numpy as np
    import torch
    import tifffile

    t4_api = _get_t4_api()
    vol_src = t4_api.volumeSource()
    if not vol_src.isLoaded():
        print('Volume not loaded')
        return

    raw = t4_api.appData().getYSlice(int(slice_number))
    input_image = (raw / 256.0).astype(np.uint8)
    if input_image.ndim == 2:
        pass
    elif input_image.ndim == 3:
        input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2GRAY)

    device = _resolve_device(params)
    model_file = params.get('model_file', 'swinir.pth')
    model_path = resolve_model_path(str(model_file))
    model = load_model(model_path, device)

    output_gray = denoise_image(model, input_image, device)

    out_u16 = (output_gray.astype(np.float32) / 255.0 * 65535.0).astype(np.uint16)
    fmt = str(params.get('output_format', 'Tiff16')).strip()

    t4_api.appData().setComponentData('VolumeOperationTool.AutoDenoiseModule', out_u16)
    filtpath = str(params.get('filtpath', '')).strip()
    if filtpath:
        out_dir = Path(filtpath)
        out_dir.mkdir(parents=True, exist_ok=True)
        if fmt == 'Tiff8':
            out_img = output_gray.astype(np.uint8)
        else:
            out_img = out_u16
        fp = out_dir / f'img_{int(slice_number):04d}.tif'
        tifffile.imwrite(str(fp), out_img)
        print(f'[Denoise] Slice file saved -> {fp.name}')
    _write_progress(params, 100)
    print(f'[Denoise] Slice {slice_number} done, shape={out_u16.shape}, dtype={out_u16.dtype}')


def process_volume(params: dict) -> None:
    import cv2
    import numpy as np
    import tifffile
    import torch

    t4_api = _get_t4_api()
    vol_src = t4_api.volumeSource()
    if not vol_src.isLoaded():
        print('Volume is not loaded')
        return

    filtpath = params.get('filtpath', '')
    if not filtpath:
        print('filtpath is required for full volume')
        return

    out_dir = Path(filtpath)
    out_dir.mkdir(parents=True, exist_ok=True)

    fmt = params.get('output_format', 'Tiff16')
    model_file = params.get('model_file', 'swinir.pth')
    device = _resolve_device(params)
    model_path = resolve_model_path(str(model_file))
    model = load_model(model_path, device)

    num_slices = vol_src.dim().y()
    _write_progress(params, 0)
    for i in range(num_slices):
        raw = vol_src.getYSlice(i, True)
        input_image = (raw / 256.0).astype(np.uint8)
        if input_image.ndim == 2:
            pass
        elif input_image.ndim == 3:
            input_image = cv2.cvtColor(input_image, cv2.COLOR_BGR2GRAY)

        output_gray = denoise_image(model, input_image, device)

        if fmt == 'Tiff8':
            out_img = output_gray.astype(np.uint8)
        else:
            out_img = (output_gray.astype(np.float32) / 255.0 * 65535.0).astype(np.uint16)

        fp = out_dir / f'img_{i:04d}.tif'
        tifffile.imwrite(str(fp), out_img)
        _write_progress(params, int((i + 1) * 100 / max(1, num_slices)))
        print(f'[Denoise] Slice {i + 1}/{num_slices} -> {fp.name}')


def main() -> None:
    _append_trace('MAIN_START argv=%r\n' % (sys.argv,))
    _debug_log('denoise.py: main(), loading torch/cv2/t4\n')

    try:
        import cv2  # noqa: F401
        import numpy as np  # noqa: F401
        import tifffile  # noqa: F401
        import torch  # noqa: F401
    except Exception:
        err = traceback.format_exc()
        try:
            with open(_TRACE, 'a', encoding='utf-8') as f:
                f.write('IMPORT FAIL\n' + err)
        except Exception:
            pass
        raise

    _debug_log('denoise.py: import OK, before volumeSource()\n')

    try:
        vol_src = _get_t4_api().volumeSource()
    except Exception:
        _debug_log('volumeSource():\n' + traceback.format_exc())
        raise

    if not vol_src.isLoaded():
        _debug_log('volume not loaded - exit without sys.exit (embedded interpreter)\n')
        print('volume is not loaded')
        return

    _pl = param_line_from_argv()
    params = parse_overrides(_pl) if _pl.strip() else {}
    try:
        if 'sliceNumber' not in params:
            params['sliceNumber'] = str(int(sliceNumber))  # noqa: F821
    except (NameError, TypeError, ValueError):
        pass
    try:
        if 'filtpath' not in params:
            _fp_glob = str(filtpath).strip()  # noqa: F821
            if _fp_glob:
                params['filtpath'] = _fp_glob
    except (NameError, TypeError, ValueError):
        pass

    try:
        _fp = filterParams  # noqa: F821
        if isinstance(_fp, str) and _fp.startswith('denoise:'):
            # denoise:swinir.pth  или  denoise:swinir.pth:Tiff16  или denoise:swinir.pth:Tiff16:CUDA
            _rest = _fp[len('denoise:') :].strip()
            _bits = _rest.split(':')
            _mf = _bits[0].strip()
            if _mf and 'model_file' not in params:
                params['model_file'] = _mf
            if len(_bits) > 1 and _bits[1].strip() and 'output_format' not in params:
                params['output_format'] = _bits[1].strip()
            if len(_bits) > 2 and _bits[2].strip() and 'device' not in params:
                params['device'] = _bits[2].strip()
    except NameError:
        pass

    if 'sliceNumber' in params and str(params.get('sliceNumber', '')).strip() != '':
        sn = int(params['sliceNumber'])
    elif 'single_slice' in params and str(params.get('single_slice', '')).strip() != '':
        sn = int(params['single_slice'])
    else:
        try:
            sn = int(sliceNumber)  # noqa: F821
        except NameError:
            sn = -999

    _log = (
        f'argv={sys.argv!r}\nparam_line={_pl!r}\nparams={params!r}\nsn={sn}\n'
    )
    _diag = _cuda_diag(params)
    _log += f"diag={_diag!r}\n"
    print('[denoise]', _log.replace('\n', ' '))
    if str(params.get('device', 'Auto')).strip().lower() == 'cuda' and _diag.get('resolved_device') != 'cuda':
        print('[denoise] WARN: CUDA requested but unavailable, fallback to CPU')
    _debug_log(_log)

    try:
        if sn >= 0:
            process_slice(params, sn)
        elif sn == -1:
            process_volume(params)
        else:
            print('[denoise] sliceNumber param is not defined.')
    except Exception:
        traceback.print_exc()
        _debug_log(_log + '\n' + traceback.format_exc())
        raise
    finally:
        _append_trace('MAIN_DONE\n')


def _fatal_log(exc: BaseException) -> None:
    tb = traceback.format_exc()
    text = f'{type(exc).__name__}: {exc}\n{tb}'
    try:
        _fatal_dir = os.path.dirname(__file__)
    except NameError:
        _fatal_dir = r'C:\t4vis\Scripts\analytics\autodenoise'
    for path in (
        _TRACE,
        _FIXED,
        os.path.join(_fatal_dir, 'denoise_fatal.txt'),
        r'C:\t4vis\Scripts\analytics\autodenoise\denoise_fatal.txt',
    ):
        try:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(text)
        except Exception:
            continue


if _should_run_entry():
    try:
        main()
    except Exception as e:
        _fatal_log(e)
        raise
