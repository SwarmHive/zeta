"""Module extensions for Zeta dependencies.

This file defines the public module extension that allows consumers
to inherit the exact versions of dependencies used by Zeta.
"""

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

def _zeta_deps_impl(_ctx):
    """Implementation of the zeta_deps module extension.
    
    This defines nats_c as the single source of truth for all consumers.
    """
    # Define nats_c here. This is the single definition used by everyone.
    git_repository(
        name = "nats_c",
        remote = "https://github.com/nats-io/nats.c.git",
        tag = "v3.9.1",
        build_file = Label("//third_party/nats:nats_c.BUILD"),
    )

# Export it as a module extension
zeta_deps = module_extension(
    implementation = _zeta_deps_impl,
)
