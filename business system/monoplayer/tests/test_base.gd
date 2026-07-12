extends Node

func assert_eq(a, b, msg: String = ""):
    if a != b:
        var err_msg = "断言失败: 期望 %s, 实际 %s" % [str(b), str(a)]
        if not msg.is_empty():
            err_msg += " - " + msg
        push_error(err_msg)

func assert_not_null(obj, msg: String = ""):
    if obj == null:
        var err_msg = "断言失败: 对象不应为 null"
        if not msg.is_empty():
            err_msg += " - " + msg
        push_error(err_msg)

func assert(condition: bool, msg: String = ""):
    if not condition:
        var err_msg = "断言失败: 条件为 false"
        if not msg.is_empty():
            err_msg += " - " + msg
        push_error(err_msg)
