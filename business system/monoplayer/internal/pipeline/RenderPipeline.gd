extends Node

var _cli_args: Dictionary = {}
var _output: OutputPort = null
var _compositor: CompositorPort = null
var _post_processes: Array[PostProcessPort] = []
var _retry_handler: RetryHandler = RetryHandler.new()
var _cam_guardian: FixedCamGuardian = null
var _anim_driver: AnimationDriver = AnimationDriver.new()
var _light_controller: LightController = LightController.new()

var _elapsed: float = 0.0
var _is_rendering: bool = false
var _is_complete: bool = false

# 架构不变量 #5: 跟踪 GPU 资源（使用 RID 而非 Godot 3.x 的 RenderTexture3D）
var _render_texture_rid: RID = RID()
var _frame_count: int = 0

func init(cli_args: Dictionary):
	_cli_args = cli_args
	
	# 设置 FixedCamGuardian（架构不变量 #1）
	_cam_guardian = FixedCamGuardian.get_instance()
	if _cam_guardian == null or _cam_guardian.get_camera() == null:
		var cam_node = get_node_or_null("/root/Root/FixedCam")
		if cam_node is Camera3D:
			var guardian = FixedCamGuardian.new()
			get_tree().current_scene.add_child(guardian)
			guardian.setup(cam_node)
			_cam_guardian = guardian
	
	# 设置 OutputPort（架构不变量 #3）
	var format = _cli_args.get("format", "prores")
	_output = OutputPort.create(format)
	if _output:
		_output.init(
			_cli_args.get("width", 1920),
			_cli_args.get("height", 1080),
			_cli_args.get("fps", 60)
		)
	
	# 初始化后处理管线（架构不变量 #3）
	_post_processes.append(ColorGrading.new())
	_post_processes.append(ContrastFilter.new())
	_post_processes.append(BloomFilter.new())
	
	# 从配置加载参数
	var config = ConfigPort.get_instance()
	if _post_processes.size() > 0 and _post_processes[0] is ColorGrading:
		(_post_processes[0] as ColorGrading).color_temperature = config.get_float("monoplayer.postprocess.color_temperature", 5500.0)
		(_post_processes[0] as ColorGrading).saturation = config.get_float("monoplayer.postprocess.saturation", 1.0)
	if _post_processes.size() > 1 and _post_processes[1] is ContrastFilter:
		(_post_processes[1] as ContrastFilter).contrast = config.get_float("monoplayer.postprocess.contrast", 1.0)
	if _post_processes.size() > 2 and _post_processes[2] is BloomFilter:
		(_post_processes[2] as BloomFilter).bloom_intensity = config.get_float("monoplayer.postprocess.bloom_intensity", 0.0)
	
	# 光源控制器从配置同步
	_light_controller.main_light_angle = config.get_float("monoplayer.lighting.main_light_angle", 45.0)
	_light_controller.main_light_intensity = config.get_float("monoplayer.lighting.main_light_intensity", 1.0)
	_light_controller.fill_light_intensity = config.get_float("monoplayer.lighting.fill_light_intensity", 0.3)
	_light_controller.rim_light_intensity = config.get_float("monoplayer.lighting.rim_light_intensity", 0.0)
	
	# 合成器
	if config.get_bool("monoplayer.compositor.enabled", false):
		_compositor = BackgroundPlate.new()
		_compositor.load_plate(config.get_string("monoplayer.compositor.plate_path", "res://assets/background_plate.png"))
		if _compositor.has_method("set_scale"):
			(_compositor as BackgroundPlate).set_scale(config.get_float("monoplayer.compositor.plate_scale", 1.0))
		if _compositor.has_method("set_opacity"):
			(_compositor as BackgroundPlate).set_opacity(config.get_float("monoplayer.compositor.plate_opacity", 1.0))
	
	# 动画驱动
	var anim_player = get_node_or_null("/root/Root/CharacterRoot/AnimationPlayer")
	if anim_player is AnimationPlayer:
		_anim_driver.setup(anim_player)
		_anim_driver.play(_cli_args.get("anim", "IdleLoop"))
	else:
		push_warning("[RenderPipeline] 未找到 AnimationPlayer，动画驱动不可用")
		print("[RenderPipeline] 检查路径: /root/Root/CharacterRoot/AnimationPlayer")
	
	# 启动输出
	if _output and not _output.start(_cli_args.get("output", "output.mov")):
		push_error("[RenderPipeline] 输出启动失败")
	
	_is_rendering = true
	print("[RenderPipeline] 渲染管线初始化完成")

func _process(delta: float):
	if not _is_rendering or _is_complete:
		return
	
	_elapsed += delta
	var duration = _cli_args.get("duration", 5.0)
	
	# 渲染当前帧
	_render_frame()
	
	# 检查是否完成
	if _elapsed >= duration:
		_finish_rendering()

func _render_frame():
	# 架构不变量 #3: 主渲染逻辑不关心输出格式
	# 只是生成 Image 并交给 OutputPort
	var viewport = get_viewport()
	if viewport:
		var image = viewport.get_texture().get_image()
		if image:
			# 后处理管线
			for pp in _post_processes:
				image = pp.apply(image)
			
			# 合成器（如果需要背景板）
			if _compositor:
				var plate = _compositor.get_plate_image() if _compositor.has_method("get_plate_image") else null
				image = _compositor.composite(image, plate)
			
			# 架构不变量 #6: RetryHandler 包装输出写入
			var output_image = image
			var write_success = _retry_handler.run(func():
				return _output.write_frame(output_image.duplicate())
			)
			if not write_success:
				push_error("[RenderPipeline] 帧写入失败: ", _retry_handler.get_last_error())
			
			_frame_count += 1

func _finish_rendering():
	_is_complete = true
	_is_rendering = false
	
	if _output:
		_output.finish()
	
	# 架构不变量 #5: GPU 资源释放
	if _render_texture_rid.is_valid():
		RenderingServer.free_rid(_render_texture_rid)
		_render_texture_rid = RID()
	
	print("[RenderPipeline] 渲染完成，共 ", _frame_count, " 帧")
	get_tree().quit()

func is_rendering() -> bool:
	return _is_rendering

func get_frame_count() -> int:
	return _frame_count
