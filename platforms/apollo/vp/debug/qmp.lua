-- SPDX-License-Identifier: BSD-3-Clause
-- Opt-in QMP uses a launcher-created mode-0700 directory. Each component owns
-- its Unix client; the monitor exposes its biflow, never the socket directly.
return function(platform, full)
    local directory = os.getenv("QBOX_APOLLO_QMP_DIR") or ""
    if directory == "" then return end
    assert(directory:sub(1, 1) == "/" and not directory:find(","),
           "QMP requires an absolute Unix socket directory without commas")
    local domains = {ap = "platform.ap_qemu_inst"}
    if full then
        domains.rse = "platform.rse_cpu_pass.qemu_inst"
        domains.si_cl0 = "platform.si_cl0_qemu_inst"
        domains.si_cl1 = "platform.si_cl1_qemu_inst"
    end
    for domain, instance in pairs(domains) do
        local path = directory.."/"..domain:gsub("_", "-")..".sock"
        assert(#path < 108, "QMP Unix socket path is too long")
        local owner = platform
        local reference = "&"..instance
        if domain == "rse" then
            -- RSE devices live in a nested container and can call get(),
            -- initializing libqemu during that container's construction.
            -- Register QMP arguments inside it, before any such device exists.
            owner = platform.rse_cpu_pass
            owner.qemu_inst_mgr.construction_priority = -103
            owner.qemu_inst.construction_priority = -102
            reference = "&qemu_inst"
        end
        owner[domain.."_qmp"] = {
            moduletype = "qmp";
            args = {reference};
            construction_priority = domain == "rse" and -101 or 0;
            monitor = false;
            qmp_str = "unix:"..path..",server=on,wait=off";
        }
    end
end
