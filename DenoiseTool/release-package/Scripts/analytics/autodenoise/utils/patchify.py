import torch
import torch.nn.functional as F
import numpy as np
from typing import Tuple, Optional

def pad_to_window_size(img_tensor: torch.Tensor, 
                       window_size: int = 8) -> Tuple[torch.Tensor, int, int]:
    _, _, h_old, w_old = img_tensor.size()
    
    h_pad = (h_old // window_size + 1) * window_size - h_old
    w_pad = (w_old // window_size + 1) * window_size - w_old
    
    padded = F.pad(img_tensor, (0, w_pad, 0, h_pad), mode='reflect')
    
    return padded, h_pad, w_pad
