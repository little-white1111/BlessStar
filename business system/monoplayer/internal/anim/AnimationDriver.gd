extends RefCounted

var _anim_player: AnimationPlayer = null
var _current_anim: String = ""
var _params: Dictionary = {}
var _is_playing: bool = false

func setup(anim_player: AnimationPlayer):
	_anim_player = anim_player

func play(anim_name: String) -> bool:
	if _anim_player == null:
		push_error("[AnimationDriver] AnimationPlayer 未设置")
		return false
	if not _anim_player.has_animation(anim_name):
		push_error("[AnimationDriver] 动画不存在: ", anim_name)
		return false
	
	_current_anim = anim_name
	_anim_player.play(anim_name)
	_is_playing = true
	print("[AnimationDriver] 播放动画: ", anim_name)
	return true

func set_param(param_name: String, value: float):
	_params[param_name] = value
	if _anim_player:
		# 使用 AnimationPlayer 的参数驱动（类似 Live2D Param）
		var method = "set" + param_name.capitalize().replace(" ", "")
		if _anim_player.has_method(method):
			_anim_player.call(method, value)
		else:
			# Fallback：使用 meta 或自定义属性
			_anim_player.set_meta(param_name, value)

func stop():
	if _anim_player and _is_playing:
		_anim_player.stop()
	_is_playing = false

func get_param(param_name: String, default: float = 0.0) -> float:
	return _params.get(param_name, default)

func is_playing() -> bool:
	return _is_playing

# 获取当前动画名称
func get_current_anim() -> String:
	return _current_anim
