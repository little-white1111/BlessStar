extends Node

# 单例实例
static var _instance: ConfigPort = null

# 接口方法（由适配器重写）
func get_float(key: String, default: float) -> float:
    return default

func get_int(key: String, default: int) -> int:
    return default

func get_string(key: String, default: String) -> String:
    return default

func get_bool(key: String, default: bool) -> bool:
    return default

# 架构不变量 #7: 必须有 fallback 默认值
# 当 BlessStarLoader 不可用时，使用 FallbackConfig 适配器

static func get_instance() -> ConfigPort:
    # 优先使用 BlessStarLoader autoload 单例（如果已注册）
    if Engine.has_singleton("BlessStarLoader"):
        var loader = Engine.get_singleton("BlessStarLoader")
        if loader is ConfigPort:
            return loader
    # 否则回退到 FallbackConfig
    if _instance == null:
        _instance = FallbackConfig.new()
    return _instance

static func set_instance(instance: ConfigPort):
    _instance = instance

# 配置变更信号（架构不变量 #4: 热更新下一帧生效）
signal config_changed(key: String, old_value, new_value)

# ===== Expression 监听支持 =====
# 注册为 ExpressionNotifier 提供订阅能力

var _expression_listeners: Array[Callable] = []

# ExpressionNotifier 通过此方法注册配置变更回调
# callback 签名: func(config_key: String, raw_value: String) -> void
func register_expression_listener(callback: Callable) -> void:
    if not callback in _expression_listeners:
        _expression_listeners.append(callback)

# 当 BlessStar 检测到 expression 相关配置变更时调用
# 会通知所有已注册的 listener
func notify_expression_change(config_key: String, raw_value: String) -> void:
    for listener in _expression_listeners:
        listener.call(config_key, raw_value)
