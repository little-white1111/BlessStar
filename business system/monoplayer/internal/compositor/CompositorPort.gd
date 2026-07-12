extends RefCounted

# 合成器端口接口
func composite(render_layer: Image, plate: Image) -> Image:
	push_error("[CompositorPort] composite() 未实现 - 抽象基类")
	return render_layer
