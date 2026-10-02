-- SPDX-License-Identifier: BSD-3-Clause
-- Execution policy for the full virtual platform.
local qvp = {}

function qvp.create()
    return {
        moduletype = "Container";
        quantum_ns = 10000000;
        keep_alive_0 = {moduletype = "keep_alive"};
    }
end

return qvp
