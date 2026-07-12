extends PostProcessPort

var contrast: float = 1.0

func apply(image: Image) -> Image:
    if contrast == 1.0:
        return image  # 跳过处理
    
    var result = image.duplicate()
    result.lock()
    
    # 安全计算对比度因子（防止极端值导致分母趋零）
    var safe_contrast = clamp(contrast, 0.0, 3.0)
    var denominator = 255.0 * (259.0 - safe_contrast * 128.0 + 255.0)
    if abs(denominator) < 0.001:
        denominator = 0.001
    var factor = (259.0 * (safe_contrast * 128.0 + 255.0)) / denominator
    
    for x in range(result.get_width()):
        for y in range(result.get_height()):
            var pixel = result.get_pixel(x, y)
            pixel.r = clamp(factor * (pixel.r - 0.5) + 0.5, 0.0, 1.0)
            pixel.g = clamp(factor * (pixel.g - 0.5) + 0.5, 0.0, 1.0)
            pixel.b = clamp(factor * (pixel.b - 0.5) + 0.5, 0.0, 1.0)
            result.set_pixel(x, y, pixel)
    
    result.unlock()
    return result
