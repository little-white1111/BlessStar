extends LightPort

var _shader_material: ShaderMaterial = null

func bind_shader(material: ShaderMaterial):
	_shader_material = material
	_sync_to_shader()

func set_main_angle(angle: float):
	main_light_angle = angle
	_sync_to_shader()

func set_main_intensity(intensity: float):
	main_light_intensity = intensity
	_sync_to_shader()

func set_fill_intensity(intensity: float):
	fill_light_intensity = intensity
	_sync_to_shader()

func set_rim_intensity(intensity: float):
	rim_light_intensity = intensity
	_sync_to_shader()

func _sync_to_shader():
	if _shader_material == null:
		return
	_shader_material.set_shader_parameter("main_light_angle", main_light_angle)
	_shader_material.set_shader_parameter("main_light_intensity", main_light_intensity)
	_shader_material.set_shader_parameter("fill_light_intensity", fill_light_intensity)
	_shader_material.set_shader_parameter("rim_light_intensity", rim_light_intensity)

func apply_to_shader(shader_material: ShaderMaterial):
	bind_shader(shader_material)
