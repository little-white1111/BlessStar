extends OutputPort

var _frame_index: int = 0
var _save_dir: String = "user://frames/"
var _base_name: String = "frame"
var _started: bool = false

func start(output_path: String) -> bool:
	# 如果 output_path 含扩展名，去掉扩展名作为目录
	if output_path.ends_with(".mp4") or output_path.ends_with(".mov") or output_path.ends_with(".avi"):
		_base_name = output_path.get_file().get_basename()
		_save_dir = output_path.get_base_dir()
	else:
		_save_dir = output_path
		_base_name = "frame"
	
	# 确保目录存在
	DirAccess.make_dir_recursive_absolute(_save_dir)
	
	_frame_index = 0
	_started = true
	print("[PNGSequence] 开始保存 PNG 序列到: ", _save_dir)
	return true

func write_frame(image: Image) -> bool:
	if not _started:
		return false
	
	var file_path = _save_dir + "/" + _base_name + "_%04d.png" % _frame_index
	var result = image.save_png(file_path)
	if result != OK:
		push_error("[PNGSequence] 保存帧失败: ", file_path)
		return false
	
	_frame_index += 1
	return true

func finish() -> bool:
	if not _started:
		return false
	_started = false
	print("[PNGSequence] PNG 序列保存完成，共 ", _frame_index, " 帧")
	print("[PNGSequence] FFmpeg 合成命令: ffmpeg -r ", fps, " -i ", _save_dir, "/", _base_name, "_%04d.png -c:v libx264 -crf 18 -pix_fmt yuva420p output_transparent.mp4")
	return true
