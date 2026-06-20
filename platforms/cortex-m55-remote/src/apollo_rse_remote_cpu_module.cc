#include "apollo_rse_remote_cpu.h"

void module_register()
{
    GSC_MODULE_REGISTER_C(ApolloRseRemoteCPU, sc_core::sc_object*);
}
