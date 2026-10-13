-- SPDX-License-Identifier: BSD-3-Clause
local apollo_dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local ctx = dofile(apollo_dir.."vp/options.lua").create(apollo_dir)
platform = dofile(apollo_dir.."vp/profiles/full-system.lua")(ctx)
