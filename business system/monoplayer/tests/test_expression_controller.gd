# test_expression_controller.gd
# ExpressionController 单元测试
# 测试覆盖：队列管理、smoothstep 插值、BlessStar 通知集成、Shader uniform 同步
extends "res://tests/test_base.gd"

var _controller: ExpressionController = null
var _mock_material: Material = null

func before_each():
    _controller = autofree(ExpressionController.new())
    _mock_material = _create_mock_material()

func test_initial_state():
    """不变量 #10: 初始状态应为中立表情，队列为空，过渡已完成"""
    var current = _controller.get_current_values()
    assert_eq(current["blush"], 0.0, "初始 blush 应为 0.0")
    assert_eq(current["eye_openness"], 1.0, "初始 eye_openness 应为 1.0")
    assert_eq(current["mouth_blend"], 0.0, "初始 mouth_blend 应为 0.0")
    assert_eq(current["brow_angle"], 0.0, "初始 brow_angle 应为 0.0")
    assert_eq(_controller.get_queue_size(), 0, "初始队列应为空")
    assert_eq(_controller.transition_progress, 1.0, "初始过渡进度应为 1.0（空闲）")

func test_enqueue_single_expression():
    """入队一个表情应触发过渡"""
    var happy_vals = {"blush": 0.3, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": -0.2}
    _controller.enqueue_expression("happy", happy_vals)
    assert_eq(_controller.get_queue_size(), 1, "入队后队列长度应为 1")

func test_queue_sequential_consumption():
    """不变量 #9: 队列应依次消费，消费后元素被移除"""
    var happy_vals = {"blush": 0.3, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": -0.2}
    var sad_vals = {"blush": 0.0, "eye_openness": 0.8, "mouth_blend": 0.7, "brow_angle": 0.3}
    _controller.enqueue_expression("happy", happy_vals)
    _controller.enqueue_expression("sad", sad_vals)
    assert_eq(_controller.get_queue_size(), 2, "两次入队后队列长度应为 2")

    # 启动第一个过渡
    _controller._start_next_transition()
    assert_eq(_controller.get_queue_size(), 1, "消费一个后面一个")

    # 模拟过渡完成
    _controller.transition_progress = 1.0
    _controller._process(0.0)
    assert_eq(_controller.get_queue_size(), 0, "过渡完成应清空队列")

func test_smoothstep_output_range():
    """不变量 #8: smoothstep 输出应始终在 [0, 1] 范围内"""
    for i in range(11):
        var t = i * 0.1  # 0.0, 0.1, ..., 1.0
        var result = _controller._smooth_step(t)
        assert_true(result >= 0.0 and result <= 1.0,
            "smoothstep(%f) = %f 应在 [0,1] 范围内" % [t, result])

func test_smoothstep_ease_properties():
    """smoothstep 的 ease-in-out 特性验证"""
    # 在 t=0.5 处，smoothstep(0.5) 应恰好等于 0.5
    assert_eq(_controller._smooth_step(0.5), 0.5, "smoothstep(0.5) 应等于 0.5")
    # 在 t=0.25 处，smoothstep(0.25) < 0.25（ease-in 段）
    assert_true(_controller._smooth_step(0.25) < 0.25,
        "smoothstep(0.25) 应小于 0.25（ease-in 段）")
    # 在 t=0.75 处，smoothstep(0.75) > 0.75（ease-out 段）
    assert_true(_controller._smooth_step(0.75) > 0.75,
        "smoothstep(0.75) 应大于 0.75（ease-out 段）")

func test_transition_progress():
    """过渡进度应随时间推进"""
    _controller.bind_material(_mock_material)
    var happy_vals = {"blush": 0.3, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": -0.2}
    _controller.enqueue_expression("happy", happy_vals)
    _controller._start_next_transition()
    
    assert_eq(_controller.transition_progress, 0.0, "过渡开始时进度应为 0.0")
    
    # 模拟 0.15s 经过（总时长 0.3s → 进度 0.5）
    _controller._process(0.15)
    assert_true(_controller.transition_progress > 0.0, "过渡进度应 > 0.0")
    assert_true(_controller.transition_progress < 1.0, "过渡进度应 < 1.0")

func test_transition_completion():
    """过渡完成应触发 expression_changed 信号"""
    var signal_fired = false
    var signal_name = ""
    _controller.expression_changed.connect(func(name): 
        signal_fired = true
        signal_name = name
    )
    
    _controller.bind_material(_mock_material)
    var happy_vals = {"blush": 0.3, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": -0.2}
    _controller.enqueue_expression("happy", happy_vals)
    _controller._start_next_transition()
    
    # 模拟过渡完成（推进 0.4s，足够 0.3s）
    _controller._process(0.4)
    
    assert_true(signal_fired, "过渡完成应发射 expression_changed 信号")
    assert_eq(signal_name, "happy", "信号应携带正确的表情名")

func test_shader_uniform_sync():
    """不变量 #10: Shader uniform 应在过渡中持续更新"""
    _controller.bind_material(_mock_material)
    
    var happy_vals = {"blush": 0.3, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": -0.2}
    _controller.enqueue_expression("happy", happy_vals)
    _controller._start_next_transition()
    
    # 中途检查 shader uniform 已更新
    _controller._process(0.05)
    var blush_val = _mock_material.get_shader_parameter("expr_blush")
    assert_true(typeof(blush_val) == TYPE_FLOAT, "expr_blush 应是 float")
    assert_true(blush_val > 0.0, "过渡中 expr_blush 应 > 0.0")

func test_blessstar_notifier_integration():
    """ExpressionNotifier 应将 BlessStar 通知正确转发到控制器"""
    var notifier = autofree(ExpressionNotifier.new())
    notifier._controller = _controller
    notifier._controller.bind_material(_mock_material)
    
    # 模拟 BlessStar 发送队列变更通知
    notifier._on_config_file_changed(
        "monoplayer.expression.queue",
        '["happy", "surprised"]'
    )
    
    assert_eq(_controller.get_queue_size(), 2, "BlessStar 通知后队列应有 2 个表情")

func test_blessstar_single_expression_swap():
    """当队列为空时，单个 expression 预设变更应直接触发过渡"""
    var notifier = autofree(ExpressionNotifier.new())
    notifier._controller = _controller
    notifier._controller.bind_material(_mock_material)
    
    # 模拟 BlessStar 通知 expression.preset 变更
    # 注意：当前 ExpressionNotifier 依赖 queue 字段来触发
    # 这里测试手动 on_config_changed 调用
    _controller.on_config_changed("monoplayer.expression.happy",
        '{"blush":0.3,"eye_openness":1.0,"mouth_blend":0.0,"brow_angle":-0.2}')
    
    assert_eq(_controller.get_queue_size(), 1, "单表情预设变更应入队")

func test_json_parse_invalid():
    """无效 JSON 不应导致崩溃，应优雅降级"""
    # 空的 raw_value
    _controller.on_config_changed("monoplayer.expression.queue", "")
    assert_eq(_controller.get_queue_size(), 0, "空 JSON 不应入队")
    
    # 非数组 JSON
    _controller.on_config_changed("monoplayer.expression.queue", '{"not":"array"}')
    assert_eq(_controller.get_queue_size(), 0, "非数组 JSON 不应入队")

func test_transition_duration_config_change():
    """过渡时长应可通过配置热更"""
    var new_duration = 0.5
    # 模拟 BlessStar 通知 transition_duration 变更
    _controller.on_config_changed("monoplayer.expression.transition_duration", str(new_duration))
    assert_eq(_controller.transition_duration, new_duration,
        "过渡时长应更新为 %f" % new_duration)

func test_concurrent_expression_queue_append():
    """过渡中追加的表情应排到队尾，不打断当前过渡"""
    _controller.bind_material(_mock_material)
    
    var happy_vals = {"blush": 0.3, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": -0.2}
    var surprised_vals = {"blush": 0.0, "eye_openness": 1.0, "mouth_blend": 0.9, "brow_angle": -0.5}
    
    _controller.enqueue_expression("happy", happy_vals)
    _controller._start_next_transition()
    
    # 过渡中追加惊讶
    _controller.enqueue_expression("surprised", surprised_vals)
    assert_eq(_controller.get_queue_size(), 1, "过渡中追加应排到队尾")
    
    # 完成当前过渡
    _controller._process(0.4)
    assert_eq(_controller.get_queue_size(), 0, "当前过渡完成后的队列长度")

func test_initial_expression_reset():
    """切换回中立表情应正确重置所有参数"""
    _controller.bind_material(_mock_material)
    var happy_vals = {"blush": 0.3, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": -0.2}
    
    _controller.enqueue_expression("happy", happy_vals)
    _controller._start_next_transition()
    _controller._process(0.4)  # 完成过渡
    
    # 切换到中立
    var neutral_vals = {"blush": 0.0, "eye_openness": 1.0, "mouth_blend": 0.0, "brow_angle": 0.0}
    _controller.enqueue_expression("neutral", neutral_vals)
    _controller._start_next_transition()
    _controller._process(0.4)
    
    var current = _controller.get_current_values()
    assert_eq(current["blush"], 0.0, "回到中立后 blush 应恢复 0.0")
    assert_eq(current["brow_angle"], 0.0, "回到中立后 brow_angle 应恢复 0.0")

# ===== 辅助方法 =====

func _create_mock_material() -> Material:
    """创建带 expression uniform 的模拟材质"""
    var mat = ShaderMaterial.new()
    var shader = Shader.new()
    shader.code = """
shader_type spatial;
uniform float expr_blush;
uniform float expr_eye_openness;
uniform float expr_mouth_blend;
uniform float expr_brow_angle;
void fragment() { ALBEDO = vec3(1.0); }
"""
    mat.shader = shader
    return mat
