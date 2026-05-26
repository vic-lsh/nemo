# include validation logic for the config options here

if(CONFIG_PEBS_SKEWNESS AND NOT CONFIG_PEBS)
    message(FATAL_ERROR "PEBS-based skewness tracking is enabled but PEBS is not")
endif()

if(CONFIG_PEBS_LOG AND NOT CONFIG_PEBS)
    message(FATAL_ERROR "PEBS logging enabled but PEBS is not")
endif()

if(CONFIG_DMA AND CONFIG_PAR_MEMCPY)
    message(FATAL_ERROR "DMA and parallel memcpy cannot be enabled together")
endif()

# ============================================================================
# Validate policy flags

# List of supported policy flags.
set(POLICY_CONFIG_FLAGS
    CONFIG_POLICY_QOS
    CONFIG_POLICY_FAIR_SHARE
    CONFIG_POLICY_RANDOM_EVICT
)

# Validate that at most one policy flag is specified.
set(POLICY_FLAG_COUNT 0)
set(SET_POLICY_FLAGS)


# Iterate through the list of variables
foreach(POLICY_VAR ${POLICY_CONFIG_FLAGS})
  if(${POLICY_VAR})
    math(EXPR POLICY_FLAG_COUNT "${POLICY_FLAG_COUNT} + 1")
    list(APPEND SET_POLICY_FLAGS ${POLICY_VAR})
  endif()
endforeach()

set(LENGTH SET_POLICY_FLAGS POLICY_FLAG_COUNT)
# Check if more than one variable is set
if(${POLICY_FLAG_COUNT} GREATER 1)
    message(FATAL_ERROR
        "Error: More than one of the following policy flags is set: ${POLICY_CONFIG_FLAGS}.\n"
        "These are the flags set: ${SET_POLICY_FLAGS}.\n"
        "Please only set one.")
endif()

# ============================================================================
