# MCAP C++ header-only library

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "mcap",
    hdrs = glob([
        "include/**/*.hpp",
    ]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [
        "@zlib",  # Optional: for compression
    ],
)