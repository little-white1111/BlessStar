extends "res://tests/test_base.gd"

func test_camera_setup():
    var guardian = FixedCamGuardian.new()
    var camera = Camera3D.new()
    guardian.setup(camera)
    assert_eq(guardian.is_locked(), true)
    assert_eq(camera.projection, Camera3D.PROJECTION_ORTHOGRAPHIC)

func test_camera_invariant_check():
    var guardian = FixedCamGuardian.new()
    var camera = Camera3D.new()
    guardian.setup(camera)
    var report = guardian.check_invariant()
    assert_eq(report["passed"], true)
    assert_eq(report["locked"], true)
