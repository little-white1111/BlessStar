extends Node

# 架构不变量 #1: 相机锁定不可违反
static var _instance: FixedCamGuardian = null
var _camera: Camera3D = null
var _locked: bool = false

func _init():
	_instance = self

func setup(camera: Camera3D):
	if _locked:
		push_warning("[FixedCamGuardian] 相机已锁定，拒绝修改")
		return
	
	_camera = camera
	_camera.current = true
	_camera.projection = Camera3D.PROJECTION_ORTHOGRAPHIC
	_camera.orthogonal_size = ConfigPort.get_instance().get_float("monoplayer.cam.orthogonal_size", 5.0)
	_camera.transform = Transform3D.IDENTITY.translated(Vector3(0, 0, -10))
	_locked = true
	
	# 禁止运行时输入/处理（架构不变量强约束）
	_camera.set_process_input(false)
	_camera.set_process(false)
	_camera.set_physics_process(false)
	
	print("[FixedCamGuardian] 正交相机已锁定: size=", _camera.orthogonal_size, ", pos=(0,0,-10)")

func lock():
	_locked = true

func is_locked() -> bool:
	return _locked

func get_camera() -> Camera3D:
	return _camera

static func get_instance() -> FixedCamGuardian:
	return _instance

# 架构不变量检查：返回一个检查报告
func check_invariant() -> Dictionary:
	if _camera == null:
		return {"passed": false, "error": "相机未初始化"}
	return {
		"passed": _locked and _camera.projection == Camera3D.PROJECTION_ORTHOGRAPHIC,
		"error": "" if _locked else "相机未锁定",
		"projection": _camera.projection,
		"locked": _locked
	}
