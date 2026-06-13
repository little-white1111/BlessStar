# ... existing content unchanged up to line 513 ...
add_library(bs_app_sdk
  app/sdk/src/app_contract.cpp
  app/sdk/src/app_ir_mapper.cpp
  app/sdk/src/app_scenario_policy.cpp
  app/sdk/src/vendor_config_normalizer.cpp
  app/sdk/src/vendor_reload_facade.cpp
  app/sdk/src/app_vendor_precheck.cpp
    app/sdk/src/app_meta_rule.cpp
  app/sdk/src/config_reload_session.cpp
  app/sdk/src/config_session_reader.cpp
  app/sdk/src/app_session.cpp
  app/sdk/src/mem_audit_log.cpp
    app/sdk/src/app_meta_rule.cpp
  # ... other existing sources may follow ...
)
# ... rest of file unchanged ...