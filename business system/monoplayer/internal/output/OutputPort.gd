extends RefCounted

# 输出端口接口 - 抽象基类
# 架构不变量 #3: 输出格式与主渲染逻辑隔离
#   新增输出格式只需实现此接口，不得修改 _process() 渲染主循环

var width: int = 1920
var height: int = 1080
var fps: int = 60

# 初始化输出
func init(w: int, h: int, f: int) -> void:
	width = w
	height = h
	fps = f

# 开始渲染输出
func start(output_path: String) -> bool:
	push_error("[OutputPort] start() 未实现 - 抽象基类")
	return false

# 写入一帧（子类实现）
func write_frame(image: Image) -> bool:
	push_error("[OutputPort] write_frame() 未实现 - 抽象基类")
	return false

# 完成输出
func finish() -> bool:
	push_error("[OutputPort] finish() 未实现 - 抽象基类")
	return false

# 工厂方法：根据 format 创建对应适配器
static func create(format: String) -> OutputPort:
	match format:
		"prores":
			return ProResExporter.new()
		"avi":
			return MovieWriter.new()
		"png_sequence":
			return PNGSequence.new()
		_:
			push_error("[OutputPort] 未知输出格式: ", format)
			return null
