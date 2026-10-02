-- SPDX-License-Identifier: BSD-3-Clause
-- AP-only profile: direct Linux or standalone U-Boot/UKIBoot first boot.
local apollo_dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
local ctx = dofile(apollo_dir.."apollo-qvp-common.lua").load(apollo_dir)
platform = dofile(apollo_dir.."vp/profiles/linux-direct.lua")(ctx)
