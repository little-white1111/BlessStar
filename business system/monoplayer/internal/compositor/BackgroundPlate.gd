extends CompositorPort

var _plate_image: Image = null
var _plate_texture: Texture2D = null
var _scale: float = 1.0
var _opacity: float = 1.0

func load_plate(path: String) -> bool:
	if FileAccess.file_exists(path):
		_plate_image = Image.load_from_file(path)
		if _plate_image == null:
			push_error("[BackgroundPlate] 加载背景板失败: ", path)
			return _generate_checkerboard()
		print("[BackgroundPlate] 背景板已加载: ", path, " (", _plate_image.get_size(), ")")
		return true
	
	# 文件不存在时自动生成棋盘格
	push_warning("[BackgroundPlate] 背景板文件不存在: ", path, "，将使用自动生成的棋盘格")
	return _generate_checkerboard()

func _generate_checkerboard(tile_size: int = 32) -> bool:
	var size = 512
	_plate_image = Image.create(size, size, false, Image.FORMAT_RGBA8)
	
	var light_color = Color(0.8, 0.8, 0.8, 1.0)   # 浅灰
	var dark_color = Color(0.6, 0.6, 0.6, 1.0)    # 深灰
	
	for x in range(size):
		for y in range(size):
			var tile_x = int(x / tile_size) % 2
			var tile_y = int(y / tile_size) % 2
			var is_light = (tile_x + tile_y) % 2 == 0
			_plate_image.set_pixel(x, y, light_color if is_light else dark_color)
	
	print("[BackgroundPlate] 棋盘格背景已生成: ", size, "x", size)
	return true

func set_scale(s: float):
	_scale = s

func set_opacity(o: float):
	_opacity = o

func composite(render_layer: Image, plate: Image) -> Image:
	# 将 render_layer（透明底）叠加到 plate 背景板上
	var result = Image.create(render_layer.get_width(), render_layer.get_height(), false, Image.FORMAT_RGBA8)
	
	# 先将背景板绘制到结果
	if plate != null:
		result.blit_rect(plate, Rect2i(0, 0, plate.get_width(), plate.get_height()), Vector2i(0, 0))
	elif _plate_image != null:
		var bg = _plate_image.duplicate()
		bg.resize(result.get_width(), result.get_height(), Image.INTERPOLATE_LANCZOS)
		result.blit_rect(bg, Rect2i(0, 0, bg.get_width(), bg.get_height()), Vector2i(0, 0))
	
	# 再将角色层 Alpha 混合上去
	result.blit_rect(render_layer, Rect2i(0, 0, render_layer.get_width(), render_layer.get_height()), Vector2i(0, 0))
	
	return result

func get_plate_image() -> Image:
	return _plate_image
