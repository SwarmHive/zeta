"""Module extensions for Zeta dependencies.

This file defines the public module extension that allows consumers
to inherit the exact versions of dependencies used by Zeta.
"""

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _zeta_deps_impl(_ctx):
    git_repository(
        name = "nats_c",
        remote = "https://github.com/nats-io/nats.c.git",
        tag = "v3.9.1",
        build_file = Label("//third_party/nats:nats_c.BUILD"),
    )

    # MCAP C++ library (header-only)
    http_archive(
        name = "mcap",
        url = "https://github.com/foxglove/mcap/archive/refs/tags/releases/cpp/v2.1.0.tar.gz",
        strip_prefix = "mcap-releases-cpp-v2.1.0/cpp/mcap",
        build_file = Label("//third_party/mcap:mcap.BUILD"),
        sha256 = "",  # Add after first fetch, or omit for now
    )       

# Export it as a module extension
zeta_deps = module_extension(
    implementation = _zeta_deps_impl,
)
