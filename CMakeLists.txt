# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(rac2_challenge_bolt_streak LANGUAGES CXX)

if(NOT OpenRACMod_DIR)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" sdk_lock)
  string(JSON sdk_version GET "${sdk_lock}" version)
  set(OpenRACMod_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${sdk_version}/cmake")
endif()
find_package(OpenRACMod 1.36 CONFIG REQUIRED)
openrac_add_mod(rac2_challenge_bolt_streak MANIFEST openrac-mod.json SOURCES bolt-streak.cpp)

include(CTest)
if(BUILD_TESTING)
  add_executable(rac2_challenge_bolt_streak_test rules_test.cpp)
  target_link_libraries(rac2_challenge_bolt_streak_test PRIVATE OpenRAC::mod)
  target_compile_features(rac2_challenge_bolt_streak_test PRIVATE cxx_std_17)
  add_test(NAME rac2_challenge_bolt_streak COMMAND rac2_challenge_bolt_streak_test)
endif()
