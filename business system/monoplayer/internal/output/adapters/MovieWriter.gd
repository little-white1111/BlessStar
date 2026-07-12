extends OutputPort

var _output_path: String = ""
var _started: bool = false

func start(output_path: String) -> bool:
	_output_path = output_path
	
	# 通过项目设置启用 MovieWriter（必须在渲染前设置）
	ProjectSettings.set_setting("editor/movie_writer/enabled", true)
	ProjectSettings.set_setting("editor/movie_writer/movie_file", _output_path)
	ProjectSettings.set_setting("editor/movie_writer/fps", fps)
	ProjectSettings.set_setting("editor/movie_writer/resolution", str(width) + "x" + str(height))
	
	_started = true
	print("[MovieWriter] AVI 输出路径: ", _output_path)
	return true

func write_frame(image: Image) -> bool:
	# MovieWriter 模式下引擎会自动处理帧写入
	# 这里只需要将帧传递给引擎的录制系统
	if not _started:
		return false
	# Godot 4.x 中 MovieWriter 由引擎内部管理，GDScript 层无需写入数据
	return true

func finish() -> bool:
	if not _started:
		return false
	_started = false
	print("[MovieWriter] 渲染完成: ", _output_path)
	return true
