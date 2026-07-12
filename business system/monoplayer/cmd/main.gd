extends Node3D

# 架构不变量 #2: CLI 参数在渲染开始后不可变
var cli_args = {}
var is_rendering = false

func _ready():
    # 1. 解析命令行参数
    _parse_cli_args()
    
    # 2. 初始化所有模块
    var render_pipeline = $"/root/Root".get_node_or_null("RenderPipeline")
    if not render_pipeline:
        render_pipeline = preload("res://internal/pipeline/RenderPipeline.gd").new()
    render_pipeline.init(cli_args)
    
    print("[monoplayer] CLI 参数已解析: ", cli_args)

func _parse_cli_args():
    var user_args = OS.get_cmdline_user_args()
    
    # 默认值
    cli_args["anim"] = "IdleLoop"
    cli_args["duration"] = 5.0
    cli_args["width"] = 1920
    cli_args["height"] = 1080
    cli_args["fps"] = 60
    cli_args["format"] = "prores"
    cli_args["output"] = "output.mov"
    
    # 解析用户参数
    var i = 0
    while i < user_args.size():
        match user_args[i]:
            "-anim": 
                i += 1
                if i < user_args.size(): cli_args["anim"] = user_args[i]
            "-duration": 
                i += 1
                if i < user_args.size(): cli_args["duration"] = float(user_args[i])
            "--resolution": 
                i += 1
                if i < user_args.size():
                    var parts = user_args[i].split("x")
                    if parts.size() == 2:
                        cli_args["width"] = int(parts[0])
                        cli_args["height"] = int(parts[1])
            "--fixed-fps": 
                i += 1
                if i < user_args.size(): cli_args["fps"] = int(user_args[i])
            "--format": 
                i += 1
                if i < user_args.size(): cli_args["format"] = user_args[i]
            "--output":
                i += 1
                if i < user_args.size(): cli_args["output"] = user_args[i]
        i += 1
    
    # 读取配置端口（可被 config-schema 覆盖）
    var config = ConfigPort.get_instance()
    if config:
        var config_anim = config.get_string("monoplayer.anim.default_name", "IdleLoop")
        var config_duration = config.get_float("monoplayer.anim.default_duration", 5.0)
        # CLI 参数优先于配置文件
        if not "-anim" in OS.get_cmdline_user_args():
            cli_args["anim"] = config_anim
        if not "-duration" in OS.get_cmdline_user_args():
            cli_args["duration"] = config_duration

# 架构不变量 #2: 渲染开始后禁止参数变更
func freeze_cli_args():
    is_rendering = true
    print("[monoplayer] CLI 参数已冻结，渲染开始")
