-- SPDX-License-Identifier: BSD-3-Clause
-- Compatibility entrypoint. New launchers use apollo-qvp-saturn-v.lua.
local apollo_dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)") or "./"
dofile(apollo_dir.."apollo-qvp-saturn-v.lua")
