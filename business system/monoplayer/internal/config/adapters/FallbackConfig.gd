extends ConfigPort

var _defaults = {
    "monoplayer.cam.orthogonal_size": 5.0,
    "monoplayer.anim.angle_x": 0.15,
    "monoplayer.anim.eye_open": 1.0,
    "monoplayer.anim.default_name": "IdleLoop",
    "monoplayer.anim.default_duration": 5.0,
    "monoplayer.output.width": 1920,
    "monoplayer.output.height": 1080,
    "monoplayer.output.fps": 60,
    "monoplayer.output.format": "prores",
    "monoplayer.output.prores_quality": "4444",
    "monoplayer.output.prores_output_path": "output.mov",
    "monoplayer.shader.toon_steps": 2,
    "monoplayer.shader.outline_width": 1.0,
    "monoplayer.lighting.main_light_angle": 45.0,
    "monoplayer.lighting.main_light_intensity": 1.0,
    "monoplayer.lighting.fill_light_intensity": 0.3,
    "monoplayer.lighting.rim_light_intensity": 0.0,
    "monoplayer.postprocess.color_temperature": 5500.0,
    "monoplayer.postprocess.saturation": 1.0,
    "monoplayer.postprocess.contrast": 1.0,
    "monoplayer.postprocess.bloom_intensity": 0.0,
    "monoplayer.compositor.enabled": false,
    "monoplayer.compositor.plate_path": "res://assets/background_plate.png",
    "monoplayer.compositor.plate_scale": 1.0,
    "monoplayer.compositor.plate_opacity": 1.0
}

func get_float(key: String, default: float) -> float:
    if _defaults.has(key) and typeof(_defaults[key]) == TYPE_FLOAT:
        return _defaults[key]
    return default

func get_int(key: String, default: int) -> int:
    if _defaults.has(key) and typeof(_defaults[key]) == TYPE_INT:
        return _defaults[key]
    return default

func get_string(key: String, default: String) -> String:
    if _defaults.has(key) and typeof(_defaults[key]) == TYPE_STRING:
        return _defaults[key]
    return default

func get_bool(key: String, default: bool) -> bool:
    if _defaults.has(key) and typeof(_defaults[key]) == TYPE_BOOL:
        return _defaults[key]
    return default
