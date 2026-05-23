#ifndef BS_TEST_CONFIG_V1_GOLDEN_H
#define BS_TEST_CONFIG_V1_GOLDEN_H

/** BlessStar Config JSON v1 golden bytes (aligned with tools/normalize/examples). */
static const char kBlessStarConfigV1Golden[] = R"({
  "kernel_version": "0.4.0",
  "adapter_version": "0.4.0",
  "manual_requirements": [],
  "instructions": [
    {
      "type": "test",
      "name": "reload-smoke-1",
      "metadata": {
        "subject_code": "1001.01",
        "tax_rate": "13"
      }
    }
  ]
})";

static const size_t kBlessStarConfigV1GoldenLen = sizeof(kBlessStarConfigV1Golden) - 1;

#endif /* BS_TEST_CONFIG_V1_GOLDEN_H */
