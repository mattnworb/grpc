package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
  name="snappy",
  hdrs = [
    "snappy.h",
    "snappy-sinksource.h",
    "snappy-c.h",
  ],
  srcs = [
    "snappy-c.cc",
    "snappy-internal.h",
    "snappy-sinksource.cc",
    "snappy-stubs-internal.cc",
    "snappy-stubs-internal.h",
    "snappy.cc",
  ],
  copts = [
#    "-Ithird_party/snappy/linux-k8",
#    "-DHAVE_CONFIG_H",
    "-Wno-unused-function"
  ],
  deps = [
  ]
)