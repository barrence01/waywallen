lito.install({
  artifacts = {
    {
      target = { kind = "lib", name = "waywallen-bridge" },
      destination = "lib/libwaywallen-bridge.so",
    },
  },
  files = {
    {
      source = "include/waywallen-bridge/bridge.h",
      destination = "include/waywallen-bridge/bridge.h",
    },
    {
      source = "include/waywallen-bridge/drm_fourcc.h",
      destination = "include/waywallen-bridge/drm_fourcc.h",
    },
    {
      source = "include/waywallen-bridge/ipc_v3.h",
      destination = "include/waywallen-bridge/ipc_v3.h",
    },
    {
      source = "include/waywallen-bridge/pool.h",
      destination = "include/waywallen-bridge/pool.h",
    },
    {
      source = "include/waywallen-bridge/probe_egl.h",
      destination = "include/waywallen-bridge/probe_egl.h",
    },
    {
      source = "include/waywallen-bridge/probe_vk.h",
      destination = "include/waywallen-bridge/probe_vk.h",
    },
    {
      source = "include/waywallen-bridge/protocol_bits.h",
      destination = "include/waywallen-bridge/protocol_bits.h",
    },
    {
      source = "include/waywallen-bridge/resolution.h",
      destination = "include/waywallen-bridge/resolution.h",
    },
  },
  pkg_config = {
    {
      target = { kind = "lib", name = "waywallen-bridge" },
      description = "C library for renderer subprocesses to talk to the waywallen daemon",
      include_directory = "include",
      dependencies = { "gbm" },
    },
  },
})
