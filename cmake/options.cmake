# cmake/options.cmake
#
# Defines the configurable build options for the project.
# This file is included by the main CMakeLists.txt.
#
# These options establish the defaults and documentation for the build. They can be
# overridden by a `config` file in the project root or by -D flags.

# Add a list variable that the main CMakeLists.txt can use for the help target.
# This makes the help target automatically aware of any new options added here.
set(PROJECT_CONFIG_OPTIONS
    CONFIG_STATS_DASHBOARD
    CONFIG_PEBS
    CONFIG_PEBS_SKEWNESS
    CONFIG_PEBS_LOG
    CONFIG_CXL_TELEM
    CONFIG_DMA
    CONFIG_PAR_MEMCPY
    # Policy flags
    CONFIG_POLICY_QOS
    CONFIG_POLICY_FAIR_SHARE
    CONFIG_POLICY_RANDOM_EVICT
    CACHE INTERNAL "List of all project-specific configuration options."
)

option(CONFIG_STATS_DASHBOARD "Enable the live-refresh dashboard in the UCM terminal" ON)

# PEBS related options
option(CONFIG_PEBS "Enable PEBS as a telemetry source" ON)
option(CONFIG_PEBS_SKEWNESS "Enable skewness tracking via PEBS samples" ON)
option(CONFIG_PEBS_LOG "Log PEBS samples to a file only" OFF) 

option(CONFIG_CXL_TELEM "Enable CXL-FPGA as a telemetry source" ON)

# memcpy options.
# if none are set, use normal memcpy.
option(CONFIG_DMA "Enable DMA offload for memcpy operations" ON)
option(CONFIG_PAR_MEMCPY "Use parallel threads to do memcpy; mutually exclusive with CONFIG_DMA" OFF)

# Policy options
option(CONFIG_POLICY_QOS "Enable QOS policy." ON) # Default policy.
option(CONFIG_POLICY_FAIR_SHARE "Enable Fair-Share policy." OFF)
option(CONFIG_POLICY_RANDOM_EVICT "Enable policy that randomly evicts fast tier pages." OFF)
