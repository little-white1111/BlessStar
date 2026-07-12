extends OutputPort

# ★ 主路径：ProRes 4444 .mov 输出
# Godot 4.x 兼容方案（无 pipe API）：
#   每帧写入原始 RGBA 字节到临时文件 →
#   渲染结束时调用 FFmpeg 编码为 ProRes 4444 .mov →
#   清理临时文件
#
# 优点：
#   - 每帧仅一次磁盘追加写入（无 PNG 压缩，性能优于 PNG 序列）
#   - FFmpeg 在渲染结束后才运行，不占用帧预算
#   - 最终产物是单文件 .mov，可直接拖入 AE/Premiere

var _temp_file_path: String = ""
var _temp_file: FileAccess = null
var _frame_count: int = 0
var _quality: String = "4444"
var _output_path: String = ""

func init(w: int, h: int, f: int, quality: String = "4444"):
	super.init(w, h, f)
	_quality = quality

func start(output_path: String) -> bool:
	_quality = ConfigPort.get_instance().get_string("monoplayer.output.prores_quality", "4444")
	_output_path = output_path
	
	# 在系统临时目录创建原始帧临时文件
	var temp_dir = OS.get_user_data_dir() + "/prores_temp/"
	DirAccess.make_dir_recursive_absolute(temp_dir)
	_temp_file_path = temp_dir + "raw_frames.rgba"
	
	_temp_file = FileAccess.open(_temp_file_path, FileAccess.WRITE)
	if _temp_file == null:
		push_error("[ProRes] 无法创建临时文件: ", _temp_file_path)
		return false
	
	print("[ProRes] 开始录制原始帧到: ", _temp_file_path)
	_frame_count = 0
	return true

func write_frame(image: Image) -> bool:
	if _temp_file == null:
		return false
	
	var raw_data = image.get_data()  # RGBA8 raw bytes
	if raw_data == null or raw_data.size() == 0:
		push_error("[ProRes] 获取图像数据失败")
		return false
	
	# 追加写入原始 RGBA 字节（纯磁盘 IO，无编码开销）
	_temp_file.store_buffer(raw_data)
	_frame_count += 1
	return true

func finish() -> bool:
	if _temp_file:
		_temp_file.close()
		_temp_file = null
	
	if _frame_count == 0:
		push_warning("[ProRes] 没有帧数据，跳过 FFmpeg 编码")
		return false
	
	print("[ProRes] 原始帧录制完成，共 ", _frame_count, " 帧，开始 FFmpeg 编码...")
	
	# 构建 FFmpeg 命令：rawvideo → ProRes 4444 .mov
	var pix_fmt = "yuva444p10le" if _quality in ["4444", "4444XQ"] else "yuv422p10le"
	var ffmpeg_args = PackedStringArray([
		"ffmpeg",
		"-y",
		"-f", "rawvideo",
		"-pix_fmt", "rgba",
		"-s", str(width) + "x" + str(height),
		"-r", str(fps),
		"-i", _temp_file_path,
		"-an",
		"-vcodec", "prores_ks",
		"-profile", _quality,
		"-pix_fmt", pix_fmt,
		_output_path
	])
	
	var output: Array = []
	var exit_code = OS.execute("ffmpeg", ffmpeg_args, output, true)
	
	if exit_code != 0:
		push_error("[ProRes] FFmpeg 编码失败 (exit=", exit_code, "): ", output)
		# 保留临时文件供人工调试
		push_warning("[ProRes] 临时文件保留在: ", _temp_file_path)
		return false
	
	# 编码成功，清理临时文件
	var dir = DirAccess.open(OS.get_user_data_dir() + "/prores_temp/")
	if dir:
		dir.remove("raw_frames.rgba")
	
	print("[ProRes] 编码完成: ", _output_path, " (", _frame_count, " 帧)")
	return true
