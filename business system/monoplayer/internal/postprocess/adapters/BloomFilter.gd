extends PostProcessPort

var bloom_intensity: float = 0.0

func apply(image: Image) -> Image:
    if bloom_intensity <= 0.0:
        return image  # 跳过处理
    
    var result = image.duplicate()
    result.lock()
    
    # 简单辉光效果：提取亮部并模糊叠加
    var threshold = 0.8  # 亮度阈值
    for x in range(result.get_width()):
        for y in range(result.get_height()):
            var pixel = result.get_pixel(x, y)
            var luminance = 0.299 * pixel.r + 0.587 * pixel.g + 0.114 * pixel.b
            if luminance > threshold:
                var bloom = (luminance - threshold) / (1.0 - threshold) * bloom_intensity * 0.3
                pixel.r = clamp(pixel.r + bloom, 0.0, 1.0)
                pixel.g = clamp(pixel.g + bloom, 0.0, 1.0)
                pixel.b = clamp(pixel.b + bloom, 0.0, 1.0)
            result.set_pixel(x, y, pixel)
    
    result.unlock()
    return result
