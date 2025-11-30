load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "nats",
    cache_entries = {
        "CMAKE_BUILD_TYPE": "Release", # Force Release build
        "NATS_BUILD_STREAMING": "OFF",
        "NATS_BUILD_EXAMPLES": "OFF",
        "NATS_BUILD_NO_SPIN": "OFF",
        "BUILD_TESTING": "OFF",
    },
    lib_source = ":all_srcs",
    out_static_libs = ["libnats_static.a"],
    out_include_dir = "include",
    linkopts = [
        "-lssl",
        "-lcrypto",
        "-lpthread",
    ],
    visibility = ["//visibility:public"],
)
