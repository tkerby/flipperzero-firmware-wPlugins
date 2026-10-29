set(PKB_HOST_WARNINGS "")
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  set(PKB_HOST_WARNINGS -Wall -Wextra)
endif()

# master_sim: model of the clock-master Game Boy for protocol tests
add_library(pkb_sim STATIC sim/master_sim.c sim/fuzz_party.c sim/mirror.c)
target_link_libraries(pkb_sim PUBLIC pkb_core)
target_include_directories(pkb_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sim)
target_compile_options(pkb_sim PRIVATE ${PKB_HOST_WARNINGS})

# One executable per unit-test file in host/tests/test_*.c.
file(GLOB PKB_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.c)
foreach(test_src ${PKB_TESTS})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} PRIVATE pkb_core pkb_sim)
  target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(${test_name} PRIVATE ${PKB_HOST_WARNINGS})
  if(NOT test_name STREQUAL "test_selfplay")
    add_test(NAME ${test_name} COMMAND ${test_name})
  endif()
endforeach()
find_package(Threads REQUIRED)
target_link_libraries(test_selfplay PRIVATE Threads::Threads)
# seed 1 to battle 11000: covers the confusion-crit game desyncs at 2257 and
# 5113 and five Beat Ups via Metronome
# (these seeds and battles predate held items in fuzzed parties)
add_test(NAME test_selfplay COMMAND test_selfplay 11000 1)
set_tests_properties(test_selfplay PROPERTIES TIMEOUT 600)
# single battles that must be flagged as game desyncs, not diverge: Pursuit
# after a Baton Pass (seed 1 battle 18117) and a Dig user's stale move struct
# after Future Sight (seed 4 battle 8223)
add_test(NAME test_selfplay_pursuit_baton_pass COMMAND test_selfplay 18118 1 18117)
add_test(NAME test_selfplay_stale_move_struct COMMAND test_selfplay 8224 4 8223)
set_tests_properties(test_selfplay test_selfplay_pursuit_baton_pass test_selfplay_stale_move_struct
  PROPERTIES ENVIRONMENT PKB_SP_NO_ITEMS=1)
# random held items
add_test(NAME test_selfplay_items COMMAND test_selfplay 11000 2)
set_tests_properties(test_selfplay_items PROPERTIES TIMEOUT 600)
# Beat Up in the parties. Seed 1 to battle 3000 has four Beat Ups run past
# the party (game desync 6, battles 380, 1612, 1943, 2661); seed 2 battle 2056
# is the wrong-status bug (game desync 5)
add_test(NAME test_selfplay_beat_up COMMAND test_selfplay 3000 1)
add_test(NAME test_selfplay_beat_up_status COMMAND test_selfplay 2057 2 2056)
set_tests_properties(test_selfplay_beat_up test_selfplay_beat_up_status PROPERTIES ENVIRONMENT PKB_SP_BEAT_UP=1)
# the AI on side 0. Fuzzed parties: it must never change the engine state
# (mirror checks); generated level-50 teams: it must beat the random policy.
add_test(NAME test_selfplay_ai COMMAND test_selfplay 2000 3)
set_tests_properties(test_selfplay_ai PROPERTIES ENVIRONMENT PKB_SP_AI=1)
add_test(NAME test_selfplay_ai_wins COMMAND test_selfplay 300 7)
set_tests_properties(test_selfplay_ai_wins PROPERTIES
  ENVIRONMENT "PKB_SP_AI=1;PKB_SP_TEAMS=50-50;PKB_SP_AI_MIN=85")

# ---- host tools ----
add_executable(dump_party tools/dump_party.c)
target_link_libraries(dump_party PRIVATE pkb_core)
target_compile_options(dump_party PRIVATE ${PKB_HOST_WARNINGS})

add_executable(replay_log tools/replay_log.c)
target_link_libraries(replay_log PRIVATE pkb_core)
target_compile_options(replay_log PRIVATE ${PKB_HOST_WARNINGS})

add_executable(sim_log tools/sim_log.c)
target_link_libraries(sim_log PRIVATE pkb_core pkb_sim)
target_compile_options(sim_log PRIVATE ${PKB_HOST_WARNINGS})

# ---- cross-checks against the Python tooling ----
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  set(PARTY_DUMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/party_dump)
  file(MAKE_DIRECTORY ${PARTY_DUMP_DIR})
  # seed:level_min:level_max
  foreach(case 1:50:50 7:75:80 99:2:100)
    string(REPLACE ":" ";" args ${case})
    list(GET args 0 seed)
    add_test(NAME dump_party_${seed} COMMAND dump_party ${PARTY_DUMP_DIR}/${seed} ${args})
    set_tests_properties(dump_party_${seed} PROPERTIES FIXTURES_SETUP party_dump_${seed})
    file(MAKE_DIRECTORY ${PARTY_DUMP_DIR}/${seed})
    add_test(NAME decode_party_${seed}
      COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/gen/decode_party.py
              ${PARTY_DUMP_DIR}/${seed}/party_block.bin ${PARTY_DUMP_DIR}/${seed}/patch.bin
              --expect ${PARTY_DUMP_DIR}/${seed}/expected.txt)
    set_tests_properties(decode_party_${seed} PROPERTIES FIXTURES_REQUIRED party_dump_${seed})
  endforeach()

  # A simulated session's battle log decodes, including the GB's party
  add_test(NAME sim_log_run COMMAND sim_log ${CMAKE_CURRENT_BINARY_DIR}/sim.pbl)
  set_tests_properties(sim_log_run PROPERTIES FIXTURES_SETUP sim_log)
  add_test(NAME decode_log
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/gen/decode_log.py
            ${CMAKE_CURRENT_BINARY_DIR}/sim.pbl)
  set_tests_properties(decode_log PROPERTIES FIXTURES_REQUIRED sim_log
    PASS_REGULAR_EXPRESSION "exchange   3: flipper .80  gb .82.*--- gb party ---.trainer GOLD.id 65057.count 3.mon 1 species 157")

  # Generated tables and movepools.bin must match a fresh regeneration
  # (fetches the pinned pokecrystal revision into tools/gen/.cache).
  add_test(NAME regen_tables
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/gen/gen_tables.py --check)
  add_test(NAME regen_movepools
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/gen/gen_movepools.py --check)
  add_test(NAME regen_trainers
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/gen/gen_trainers.py --check)
  set_tests_properties(regen_tables regen_movepools regen_trainers PROPERTIES LABELS regen TIMEOUT 600)
endif()

# ---- SameBoy harness (needs the SameBoy submodule; ROM supplied at run time) ----
if(EXISTS ${PROJECT_SOURCE_DIR}/host/third_party/SameBoy/Core/gb.c)
  add_subdirectory(harness)
endif()
