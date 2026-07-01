#include "apollo_rse_cpu.h"

void module_register()
{
    GSC_MODULE_REGISTER_C(ApolloRseCPU, sc_core::sc_object*);
}
