extends ExpressionPort
class_name ExpressionController

# ExpressionController — 表情控制核心控制器
#
# Design: BlessStar → config change → on_config_changed → queue → smoothstep → shader
#
# Key concepts:
# 1. BlessStar only notifies "what's next" (not per-frame param driving)
# 2. Queue supports sequential expression playback
# 3. Interpolation happens in _process() via smoothstep (ease-in-out)
# 4. Shader uniforms update every frame during transition

# 表情预设值（neutral / happy / sad / angry / surprised）
var expression_presets: Dictionary = {
	"neutral": { "blush": 0.0, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": 0.0 },
	"happy": { "blush": 0.3, "eye_openness": 0.95, "mouth_blend": 0.7, "brow_angle": 0.2 },
	"sad": { "blush": 0.0, "eye_openness": 0.6, "mouth_blend": 0.2, "brow_angle": -0.4 },
	"angry": { "blush": 0.0, "eye_openness": 0.85, "mouth_blend": 0.1, "brow_angle": 0.6 },
	"surprised": { "blush": 0.0, "eye_openness": 1.0, "mouth_blend": 0.6, "brow_angle": 0.7 }
}

# 当前混合值（每帧插值更新）
var current_values: Dictionary = {
	"blush": 0.0,
	"eye_openness": 1.0,
	"mouth_blend": 0.0,
	"brow_angle": 0.0
}

# 目标表情值（当前过渡终点）
var target_values: Dictionary = {
	"blush": 0.0,
	"eye_openness": 1.0,
	"mouth_blend": 0.0,
	"brow_angle": 0.0
}

# 表情队列 — 元素格式: { name: String, values: Dictionary }
var expression_queue: Array[Dictionary] = []

# 过渡状态
var transition_progress: float = 1.0   # 1.0 = 无活跃过渡
var transition_duration: float = 0.5   # 秒（可由 config 覆盖）

# 过渡起始值快照（插值起点）
var _start_values: Dictionary = {}

# 绑定的 Shader 材质
var _material: Material = null


func _init():
	var neutral = expression_presets.get("neutral", {})
	for key in current_values.keys():
		current_values[key] = neutral.get(key, current_values[key])
		target_values[key] = neutral.get(key, target_values[key])


# --- ExpressionPort 接口实现 ---

func bind_material(material: Material) -> void:
	_material = material
	_sync_shader_uniforms()


func on_config_changed(config_key: String, raw_value: String) -> void:
	match config_key:
		"expression.preset":
			var preset_name = raw_value.strip_edges().to_lower()
			if expression_presets.has(preset_name):
				enqueue_expression(preset_name, expression_presets[preset_name])
			else:
				push_warning("[ExpressionController] 未知表情预设: ", preset_name)

		"expression.values":
			var parsed = _parse_json(raw_value)
			if parsed is Dictionary and not parsed.is_empty():
				enqueue_expression("custom", parsed)

		"expression.queue":
			_parse_queue(raw_value)

		"expression.transition_duration":
			var dur = float(raw_value)
			if dur > 0.0:
				transition_duration = dur


func enqueue_expression(name: String, values: Dictionary) -> void:
	expression_queue.append({ "name": name, "values": values.duplicate() })
	# 如果当前没有活跃过渡，立即开始
	if expression_queue.size() == 1 and transition_progress >= 1.0:
		_start_next_transition()


func get_current_values() -> Dictionary:
	return current_values.duplicate()


func get_queue_size() -> int:
	return expression_queue.size()


# --- 插值函数 ---

# smoothstep 缓入缓出插值
func _smooth_step(t: float) -> float:
	return t * t * (3.0 - 2.0 * t)


# 将 current_values 同步到 Shader uniform（前缀 "expr_"）
func _sync_shader_uniforms() -> void:
	if not _material is ShaderMaterial:
		return
	var sm: ShaderMaterial = _material as ShaderMaterial
	sm.set_shader_parameter("expr_blush", current_values.get("blush", 0.0))
	sm.set_shader_parameter("expr_eye_openness", current_values.get("eye_openness", 1.0))
	sm.set_shader_parameter("expr_mouth_blend", current_values.get("mouth_blend", 0.0))
	sm.set_shader_parameter("expr_brow_angle", current_values.get("brow_angle", 0.0))


# 主循环：驱动过渡进度并更新 shader
func _process(delta: float) -> void:
	if transition_progress >= 1.0:
		return

	# 推进进度
	transition_progress = min(transition_progress + delta / transition_duration, 1.0)
	var t = _smooth_step(transition_progress)

	# 插值所有通道
	for key in current_values.keys():
		if _start_values.has(key) and target_values.has(key):
			current_values[key] = lerpf(_start_values[key], target_values[key], t)

	_sync_shader_uniforms()

	# 过渡完成
	if transition_progress >= 1.0:
		# 精确对齐目标值
		for key in target_values.keys():
			current_values[key] = target_values[key]
		_sync_shader_uniforms()

		# 如果队列中还有条目，启动下一个
		if not expression_queue.is_empty():
			_start_next_transition()


# --- 队列管理 ---

# 启动下一个过渡：从队列中弹出一个条目
func _start_next_transition() -> void:
	if expression_queue.is_empty():
		return
	var entry: Dictionary = expression_queue.pop_front()
	target_values = entry.get("values", {}).duplicate()
	_start_values = current_values.duplicate()
	transition_progress = 0.0
	expression_changed.emit(entry.get("name", "unknown"))


# 解析队列 JSON（支持数组格式和单个条目）
func _parse_queue(raw_value: String) -> void:
	var parsed = _parse_json(raw_value)
	if parsed is Array:
		for item in parsed:
			if item is Dictionary:
				var name_val: String = item.get("name", "custom")
				var values_val: Dictionary = item.get("values", {})
				if not values_val.is_empty():
					expression_queue.append({ "name": name_val, "values": values_val.duplicate() })
				elif item.has("preset"):
					var preset_name = str(item["preset"]).to_lower()
					if expression_presets.has(preset_name):
						expression_queue.append({ "name": preset_name, "values": expression_presets[preset_name].duplicate() })
		# 如果队列之前为空，立即启动第一个
		if expression_queue.size() > 0 and transition_progress >= 1.0:
			_start_next_transition()
	elif parsed is Dictionary:
		# 单个条目
		var name_val: String = parsed.get("name", "custom")
		var values_val: Dictionary = parsed.get("values", {})
		if not values_val.is_empty():
			enqueue_expression(name_val, values_val)


# 安全的 JSON 解析，解析失败返回空 Dictionary
func _parse_json(raw: String) -> Variant:
	var result = JSON.parse_string(raw.strip_edges())
	if result == null:
		push_warning("[ExpressionController] JSON 解析失败: ", raw)
		return {}
	return result
