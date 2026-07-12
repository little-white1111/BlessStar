extends RefCounted

# 光源配置（可热更新）
var main_light_angle: float = 45.0
var main_light_intensity: float = 1.0
var fill_light_intensity: float = 0.3
var rim_light_intensity: float = 0.0

func apply_to_shader(shader_material: ShaderMaterial):
	pass  # 由具体适配器实现
