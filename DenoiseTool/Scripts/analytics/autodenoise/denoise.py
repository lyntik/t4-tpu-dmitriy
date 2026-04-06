import sys
import torch
import numpy as np
import cv2
from pathlib import Path
from models.network_swinir import SwinIR
from utils.patchify import pad_to_window_size

def parse_overrides(overrides_str):
    """Парсинг параметров из C++"""
    params = {}
    for item in overrides_str.split(','):
        if '=' in item:
            key, value = item.split('=', 1)
            key = key.strip().strip("'")
            value = value.strip().strip("'")
            params[key] = value
    return params

def load_model(model_name, device='cuda'):
    model = SwinIR(
        img_size=128,
        in_chans=1,
        embed_dim=180,
        depths=[6, 6, 6, 6, 6, 6],
        num_heads=[6, 6, 6, 6, 6, 6],
        window_size=8,
        mlp_ratio=2,
        img_range=1.,
        upsampler='',
        resi_connection='1conv',
        upscale=1,
    )
    
    model_path = Path(__file__).parent / 'models' / f'{model_name.lower()}.pth'
    if model_path.exists():
        checkpoint = torch.load(model_path, map_location=device)
        model.load_state_dict(checkpoint)
        print(f'[Model] {model_name} loaded')
    else:
        print(f'[Warning] Model file not found: {model_path}')
    
    model.to(device)
    model.eval()
    return model

def denoise_image(model, image, device='cuda'):
    """Выполнение денoйзинга"""
    with torch.no_grad():
        # Нормализация
        img_tensor = torch.from_numpy(image).float() / 255.0
        img_tensor = img_tensor.permute(2, 0, 1).unsqueeze(0).to(device)
        
        # Инференс
        output = tile_process(img_tensor, model, 128, 32, 8)
        
        # Денормализация
        output = output.squeeze(0).permute(1, 2, 0).cpu().numpy()
        output = np.clip(output * 255, 0, 255).astype(np.uint8)
        
    return output

def process_slice(params, slice_number):
    """Обработка одного среза"""
    import t4  # Tomograph4 API
    
    volSrc = t4.volumeSource()
    if not volSrc.isLoaded():
        print('Volume not loaded')
        return
    
    # Получение среза
    input_image = t4.appData().getYSlice(int(slice_number))
    input_image = (input_image / 256).astype(np.uint8)
    
    # Загрузка модели
    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    model = load_model(params.get('model_name', 'swinir'), device)
    
    # Денoйзинг
    output_image = denoise_image(model, input_image, device)
    
    # Возврат результата в C++
    t4.appData().setComponentData('VolumeOperationTool.AutoDenoiseModule', 
                                   output_image)
    print(f'[Denoise] Slice {slice_number} processed')

def process_volume(params):
    """Обработка всего объема"""
    import t4
    
    volSrc = t4.volumeSource()
    if not volSrc.isLoaded():
        print('Volume is not loaded')
        return
    
    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    model = load_model(params.get('model_name', 'swinir'), device)
    
    num_slices = volSrc.dim().y()
    
    for i in range(num_slices):
        input_image = volSrc.getYSlice(i, True)
        input_image = (input_image / 256).astype(np.uint8)
        
        if len(input_image.shape) == 2:
            input_image = cv2.cvtColor(input_image, cv2.COLOR_GRAY2RGB)
        
        output_image = denoise_image(model, input_image, device)
        
        print(f'[Denoise] Slice {i}/{num_slices} processed')


def tile_process(img_tensor: torch.Tensor, 
                 model: torch.nn.Module,
                 tile_size: Optional[int] = None,
                 tile_overlap: int = 32,
                 window_size: int = 8) -> torch.Tensor:
    if tile_size is None:
        # Обработка целиком
        padded, h_pad, w_pad = pad_to_window_size(img_tensor, window_size)
        with torch.no_grad():
            output = model(padded)
        # Обрезаем паддинг
        _, _, h_old, w_old = img_tensor.size()
        output = output[..., :h_old, :w_old]
        return output
    
    # Обработка по тайлам
    b, c, h, w = img_tensor.size()
    tile = min(tile_size, h, w)
    
    # Tile должен быть кратен window_size
    tile = (tile // window_size) * window_size
    
    stride = tile - tile_overlap
    
    # Индексы для нарезки
    h_idx_list = list(range(0, h - tile, stride)) + [h - tile]
    w_idx_list = list(range(0, w - tile, stride)) + [w - tile]
    
    # Буферы для результата и весов
    E = torch.zeros(b, c, h, w, dtype=img_tensor.dtype, device=img_tensor.device)
    W = torch.zeros_like(E)
    
    with torch.no_grad():
        for h_idx in h_idx_list:
            for w_idx in w_idx_list:
                in_patch = img_tensor[..., h_idx:h_idx+tile, w_idx:w_idx+tile]
                
                padded, h_pad, w_pad = pad_to_window_size(in_patch, window_size)
            
                out_patch = model(padded)
                
                out_patch = out_patch[..., :tile, :tile]
                
                out_patch_mask = torch.ones_like(out_patch)
                
                E[..., h_idx:h_idx+tile, w_idx:w_idx+tile] += out_patch
                W[..., h_idx:h_idx+tile, w_idx:w_idx+tile] += out_patch_mask
    
    output = E / W
    
    return output

def main():
    if len(sys.argv) < 2:
        print('Usage: denoise.py <overrides>')
        return
    
    params = parse_overrides(sys.argv[1])
    single_slice = int(params.get('single_slice', -1))
    
    if single_slice >= 0:
        process_slice(params, single_slice)
    else:
        process_volume(params)

if __name__ == "__main__":
    main()