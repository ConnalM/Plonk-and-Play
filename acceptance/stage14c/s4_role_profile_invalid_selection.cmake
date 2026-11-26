if(NOT DEFINED PP_S4_PROFILE_FILE)
  message(FATAL_ERROR "PP_S4_PROFILE_FILE is required")
endif()

include("${PP_S4_PROFILE_FILE}")
pp_s4_select_role("INVALID" invalid_profile)
