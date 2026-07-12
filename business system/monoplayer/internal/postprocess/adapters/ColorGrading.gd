extends PostProcessPort

var color_temperature: float = 5500.0
var saturation: float = 1.0

func apply(image: Image) -> Image:
    if color_temperature == 5500.0 and saturation == 1.0:
        return image  # 跳过处理
    
    var result = image.duplicate()
    result.lock()
    
    for x in range(result.get_width()):
        for y in range(result.get_height()):
            var pixel = result.get_pixel(x, y)
            
            # 色温调节（简化实现：R/B 通道偏移）
            var temp_factor = (color_temperature - 5500.0) / 5500.0
            pixel.r = clamp(pixel.r + temp_factor * 0.1, 0.0, 1.0)
            pixel.b = clamp(pixel.b - temp_factor * 0.1, 0.0, 1.0)
            
            # 饱和度调节
            var gray = (pixel.r + pixel.g + pixel.b) / 3.0
            pixel.r = lerp(gray, pixel.r, saturation)
            pixel.g = lerp(gray, pixel.g, saturation)
            pixel.b = lerp(gray, pixel.b, saturation)
            
            result.set_pixel(x, y, pixel)
    
    result.unlock()
    return result
