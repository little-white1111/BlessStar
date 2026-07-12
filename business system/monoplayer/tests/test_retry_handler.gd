extends "res://tests/test_base.gd"

func test_retry_success_first_try():
    var handler = RetryHandler.new()
    var attempt = 0
    var result = handler.run(func():
        attempt += 1
        return true
    )
    assert_eq(result, true)
    assert_eq(attempt, 1)

func test_retry_fails_then_succeeds():
    var handler = RetryHandler.new()
    var attempt = 0
    var result = handler.run(func():
        attempt += 1
        return attempt >= 2
    )
    assert_eq(result, true)
    assert_eq(attempt, 2)

func test_retry_max_attempts():
    var handler = RetryHandler.new()
    var attempt = 0
    var result = handler.run(func():
        attempt += 1
        return false
    )
    assert_eq(result, false)
    assert_eq(attempt, 3)  # 架构不变量 #6: 最多 3 次

func test_retry_reset():
    var handler = RetryHandler.new()
    handler.run(func(): return false)
    assert_eq(handler.get_attempts(), 3)
    handler.reset()
    assert_eq(handler.get_attempts(), 0)
