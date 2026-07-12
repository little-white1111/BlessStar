extends Node
class_name ExpressionNotifier

# ExpressionNotifier — BlessStar ↔ ExpressionController 桥接器
#
# 通过 ConfigPort 监听 config 文件变更，将变更转发给 ExpressionController。
# 当 BlessStar 检测到配置变化时，触发 _on_config_file_changed 回调，
# 将 config_key 和 raw_value 原样传递给 ExpressionController.on_config_changed()。
#
# 流程:
#   BlessStar 热更新配置
#     → ConfigPort._expression_listeners 通知
#       → ExpressionNotifier._on_config_file_changed
#         → ExpressionController.on_config_changed(config_key, raw_value)
#           → 解析 & 入队 & 平滑过渡

@onready var _config_port: ConfigPort = ConfigPort.get_instance()
var _controller: ExpressionController = null


# 设置关联的 ExpressionController 实例
func setup(controller: ExpressionController) -> void:
	_controller = controller


func _ready() -> void:
	if _config_port:
		_config_port.register_expression_listener(_on_config_file_changed)
	else:
		push_warning("[ExpressionNotifier] ConfigPort 不可用，无法注册监听")


# 当 BlessStar 检测到配置变更时触发
func _on_config_file_changed(config_key: String, raw_value: String) -> void:
	if _controller == null:
		push_warning("[ExpressionNotifier] ExpressionController 未设置，忽略变更: ", config_key)
		return
	_controller.on_config_changed(config_key, raw_value)
