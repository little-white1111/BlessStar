extends RefCounted

const MAX_RETRIES := 3  # 架构不变量 #6: 硬约束，不可配置
var _attempt_count: int = 0
var _last_error: String = ""

func run(task: Callable) -> bool:
    _attempt_count = 0
    _last_error = ""
    while _attempt_count < MAX_RETRIES:
        _attempt_count += 1
        var result = task.call()
        if result is bool and result:
            return true
        _last_error = "Attempt " + str(_attempt_count) + " failed"
        print("[Retry] ", _last_error, ", retrying...")
    return false

func get_attempts() -> int:
    return _attempt_count

func get_last_error() -> String:
    return _last_error

# 测试用：重置状态
func reset():
    _attempt_count = 0
    _last_error = ""
