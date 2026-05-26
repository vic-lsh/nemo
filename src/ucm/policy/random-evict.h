#ifndef HEMEM_RANDOM_EVICT_H
#define HEMEM_RANDOM_EVICT_H

#include "policy-interface.h"
#include "policy/policy.h"

int random_evict_policy_init(policy_t* p, struct policy_opts* opts);

#endif /* End of HEMEM_RANDOM_EVICT_H */
