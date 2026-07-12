extends Node
class_name ExpressionPort

# ExpressionPort — 表情控制端口接口
# 定义表情控制模块的信号与抽象方法契约

# 表情变更信号（表情名称、当前值）
signal expression_changed(name: String)

# 绑定目标 Toon Shader 材质
func bind_material(material: Material) -> void:
	push_error("[ExpressionPort] bind_material() 未实现 - 抽象基类")

# 由 BlessStar 驱动：收到配置变更后转发到队列
func on_config_changed(config_key: String, raw_value: String) -> void:
	push_error("[ExpressionPort] on_config_changed() 未实现 - 抽象基类")

# 手动入队一组表情值
func enqueue_expression(name: String, values: Dictionary) -> void:
	push_error("[ExpressionPort] enqueue_expression() 未实现 - 抽象基类")

# 获取当前混合后的表情值
func get_current_values() -> Dictionary:
	push_error("[ExpressionPort] get_current_values() 未实现 - 抽象基类")
	return {}

# 获取剩余队列长度
func get_queue_size() -> int:
	push_error("[ExpressionPort] get_queue_size() 未实现 - 抽象基类")
	return 0
