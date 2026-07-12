extends "res://tests/test_base.gd"

func test_fallback_config_defaults():
    var config = FallbackConfig.new()
    assert_eq(config.get_float("monoplayer.cam.orthogonal_size", 0.0), 5.0)
    assert_eq(config.get_int("monoplayer.output.width", 0), 1920)
    assert_eq(config.get_string("monoplayer.anim.default_name", ""), "IdleLoop")
    assert_eq(config.get_bool("monoplayer.compositor.enabled", null), false)

func test_fallback_config_missing_key():
    var config = FallbackConfig.new()
    assert_eq(config.get_float("nonexistent.key", 42.0), 42.0)

func test_config_singleton():
    ConfigPort.set_instance(null)
    var instance = ConfigPort.get_instance()
    assert_not_null(instance)
    assert(instance is FallbackConfig)
