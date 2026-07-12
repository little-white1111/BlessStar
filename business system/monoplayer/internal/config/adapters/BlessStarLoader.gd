extends ConfigPort

var _config_path: String = "res://config-schema.yaml"
var _last_mtime: int = 0
var _cache: Dictionary = {}
var _poll_timer: float = 0.0
var _poll_interval: float = 1.0  # 每秒检查一次

func _init(path: String = "res://config-schema.yaml"):
	_config_path = path
	_reload()

func _process(delta: float):
	_poll_timer += delta
	if _poll_timer >= _poll_interval:
		_poll_timer = 0.0
		_check_and_reload()

func _check_and_reload():
	var file = FileAccess.open(_config_path, FileAccess.READ)
	if file == null:
		return
	var mtime = file.get_modified_time()
	file.close()
	if mtime != _last_mtime:
		_reload()
		_last_mtime = mtime

func _reload():
	var file = FileAccess.open(_config_path, FileAccess.READ)
	if file == null:
		push_warning("[BlessStar] 无法打开配置: ", _config_path)
		return
	# 简易 YAML 解析（仅支持 field.value: scalar 格式）
	while not file.eof_reached():
		var line = file.get_line().strip_edges()
		if line.begins_with("#") or line.is_empty():
			continue
		var parts = line.split(":", true, 1)
		if parts.size() == 2:
			var key = parts[0].strip_edges()
			var val = parts[1].strip_edges().trim_prefix("\"").trim_suffix("\"")
			_cache[key] = val
	file.close()
	print("[BlessStar] 配置已加载: ", _config_path, " (", _cache.size(), " 个字段)")

func _parse_numeric_value(raw: String) -> float:
	raw = raw.strip_edges()
	# 处理 DURATION 类型（如 "5s" → 5.0）
	if raw.length() > 0 and raw[raw.length() - 1] in ["s", "S"]:
		raw = raw.substr(0, raw.length() - 1)
	# 处理单位后缀 m/h（分钟/小时）
	elif raw.length() > 0 and raw[raw.length() - 1] in ["m", "M"]:
		raw = raw.substr(0, raw.length() - 1)
		return float(raw) * 60.0
	elif raw.length() > 0 and raw[raw.length() - 1] in ["h", "H"]:
		raw = raw.substr(0, raw.length() - 1)
		return float(raw) * 3600.0
	return float(raw)

func get_float(key: String, default: float) -> float:
	if _cache.has(key):
		return _parse_numeric_value(_cache[key])
	return default

func get_int(key: String, default: int) -> int:
	if _cache.has(key):
		return int(_cache[key])
	return default

func get_string(key: String, default: String) -> String:
	if _cache.has(key):
		return str(_cache[key])
	return default

func get_bool(key: String, default: bool) -> bool:
	if _cache.has(key):
		var v = _cache[key].to_lower()
		return v == "true" or v == "yes" or v == "1"
	return default
