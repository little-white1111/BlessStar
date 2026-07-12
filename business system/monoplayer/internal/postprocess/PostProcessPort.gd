extends RefCounted

# 后处理端口接口 - 架构不变量 #3: 新增后处理不得修改主循环
func apply(image: Image) -> Image:
    push_error("[PostProcessPort] apply() 未实现 - 抽象基类")
    return image
