/*
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <host_gtimer.h>

#include <cstring>
#include <iomanip>
#include <iostream>

void host_gtimer::trace_access(tlm::tlm_generic_payload& trans,
                               uint64_t offset, unsigned int len, bool debug)
{
    if (!p_trace.get_value() || m_trace_count >= p_trace_limit.get_value()) {
        return;
    }
    ++m_trace_count;
    uint32_t value = 0;
    if (len <= sizeof(value)) {
        std::memcpy(&value, trans.get_data_ptr(), len);
    }
    std::cerr << name() << " " << (debug ? "dbg_" : "")
              << (trans.is_read() ? "read" : "write")
              << " offset=0x" << std::hex << offset << " len=0x" << len
              << " value=0x" << value << std::dec << std::endl;
}
