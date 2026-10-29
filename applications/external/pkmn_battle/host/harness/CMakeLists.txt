# SameBoy in-process harness: drives a real Crystal ROM against our link
# session (or against a second emulated Game Boy). Needs the user's own ROM
# at run time; nothing here is run by ctest unless PKB_ROM is set.

set(SAMEBOY_DIR ${PROJECT_SOURCE_DIR}/host/third_party/SameBoy)
file(STRINGS ${SAMEBOY_DIR}/version.mk SAMEBOY_VERSION_LINE REGEX "^VERSION")
string(REGEX REPLACE "^VERSION *:= *" "" SAMEBOY_VERSION "${SAMEBOY_VERSION_LINE}")

# The emulation core only, as in SameBoy's libretro build (no debugger,
# cheats, rewind or frame-rate sync).
add_library(sameboy_core STATIC
  ${SAMEBOY_DIR}/Core/gb.c
  ${SAMEBOY_DIR}/Core/sgb.c
  ${SAMEBOY_DIR}/Core/apu.c
  ${SAMEBOY_DIR}/Core/memory.c
  ${SAMEBOY_DIR}/Core/mbc.c
  ${SAMEBOY_DIR}/Core/timing.c
  ${SAMEBOY_DIR}/Core/display.c
  ${SAMEBOY_DIR}/Core/camera.c
  ${SAMEBOY_DIR}/Core/sm83_cpu.c
  ${SAMEBOY_DIR}/Core/joypad.c
  ${SAMEBOY_DIR}/Core/save_state.c
  ${SAMEBOY_DIR}/Core/random.c
  ${SAMEBOY_DIR}/Core/rumble.c)
set_target_properties(sameboy_core PROPERTIES C_EXTENSIONS ON)
target_include_directories(sameboy_core PUBLIC ${SAMEBOY_DIR})
target_compile_definitions(sameboy_core
  PUBLIC GB_DISABLE_TIMEKEEPING GB_DISABLE_REWIND GB_DISABLE_DEBUGGER GB_DISABLE_CHEATS
         GB_DISABLE_CHEAT_SEARCH
  PRIVATE GB_INTERNAL _GNU_SOURCE _USE_MATH_DEFINES
          GB_VERSION="${SAMEBOY_VERSION}" GB_COPYRIGHT_YEAR="2025")
if(WIN32)
  target_compile_definitions(sameboy_core PRIVATE random=rand)
else()
  target_link_libraries(sameboy_core PUBLIC m) # sgb.c/apu.c use libm
endif()
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  # Always optimized: fuzz runs emulate millions of frames.
  target_compile_options(sameboy_core PRIVATE -O2 -w)
endif()

add_library(pkb_harness_lib STATIC
  gbh.c
  sym.c
  boot_stub.c
  bridge.c
  driver.c
  engine_session.c
  flipper.c
  shadow.c
  trace.c)
target_include_directories(pkb_harness_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${PROJECT_SOURCE_DIR}/host/tests)
target_link_libraries(pkb_harness_lib PUBLIC sameboy_core pkb_core pkb_sim)
target_compile_options(pkb_harness_lib PRIVATE ${PKB_HOST_WARNINGS})

add_executable(pkb_harness harness_main.c)
target_link_libraries(pkb_harness PRIVATE pkb_harness_lib)
target_compile_options(pkb_harness PRIVATE ${PKB_HOST_WARNINGS})

# ---- emulator tests (opt in: they need your own ROM and save) ----
#   cmake -B build-host -DPKB_ROM=local/crystal.gbc -DPKB_SAV=local/crystal.srm
#         [-DPKB_REPLAY_LOGS=<dir of device .pbl logs>]
# The save must have been made facing the Colosseum receptionist.
set(PKB_ROM "" CACHE FILEPATH "Pokemon Crystal (US/EU 1.0) ROM for the harness tests")
set(PKB_SAV "" CACHE FILEPATH "Battery save made facing the Colosseum receptionist")
set(PKB_REPLAY_LOGS "" CACHE PATH "Directory of device battle logs (.pbl) to replay")
if(PKB_ROM AND PKB_SAV)
  get_filename_component(rom ${PKB_ROM} ABSOLUTE BASE_DIR ${PROJECT_SOURCE_DIR})
  get_filename_component(sav ${PKB_SAV} ABSOLUTE BASE_DIR ${PROJECT_SOURCE_DIR})
  set(state ${CMAKE_CURRENT_BINARY_DIR}/colosseum.state)
  set(gb --rom ${rom} --state ${state} --quiet)

  add_test(NAME harness_fixture COMMAND pkb_harness fixture --rom ${rom} --sav ${sav} --out ${state})
  set_tests_properties(harness_fixture PROPERTIES FIXTURES_SETUP colosseum LABELS harness)

  # five console rematches with fresh teams, twice, byte-identical
  add_test(NAME harness_rematches COMMAND pkb_harness run ${gb} --battles 5 --repeat 2)
  set(harness_tests harness_rematches)
  # our session against a real (emulated) slave GB
  foreach(seed 1 2 3)
    add_test(NAME harness_golden_${seed} COMMAND pkb_harness golden ${gb} --seed ${seed})
    list(APPEND harness_tests harness_golden_${seed})
  endforeach()
  # the battle engine: against the slave GB's own state (shadow, also
  # seeded through pkb_engine_init), and as on the device through our
  # session, whose logs must then replay in the engine alone
  add_test(NAME harness_shadow COMMAND pkb_harness shadow ${gb} --battles 4 --seed 11)
  add_test(NAME harness_shadow_init COMMAND pkb_harness shadow ${gb} --battles 4 --seed 12 --init)
  # regression corpus: the first battles of each effect wave's soak seeds
  # (--no-items: these seeds predate held items and gave parties without held items)
  add_test(NAME harness_shadow_w1 COMMAND pkb_harness shadow ${gb} --battles 4 --seed 201 --no-items)
  add_test(NAME harness_shadow_w1_init COMMAND pkb_harness shadow ${gb} --battles 4 --seed 301 --init --no-items)
  # (606: battle 2 has a Future Sight that fails, which leaves DoMove early)
  add_test(NAME harness_shadow_w2 COMMAND pkb_harness shadow ${gb} --battles 4 --seed 501 --no-items)
  add_test(NAME harness_shadow_w2_init COMMAND pkb_harness shadow ${gb} --battles 3 --seed 606 --init --no-items)
  add_test(NAME harness_shadow_w3 COMMAND pkb_harness shadow ${gb} --battles 4 --seed 702)
  add_test(NAME harness_shadow_w3_init COMMAND pkb_harness shadow ${gb} --battles 4 --seed 801 --init)
  set(engine_logs ${CMAKE_CURRENT_BINARY_DIR}/engine_logs)
  file(MAKE_DIRECTORY ${engine_logs})
  add_test(NAME harness_engine COMMAND pkb_harness engine ${gb} --battles 4 --seed 13 --out ${engine_logs})
  set_tests_properties(harness_engine PROPERTIES FIXTURES_SETUP engine_logs)
  foreach(i 00 01 02 03)
    add_test(NAME harness_engine_replay_${i} COMMAND replay_log ${engine_logs}/battle${i}.pbl)
    set_tests_properties(harness_engine_replay_${i} PROPERTIES FIXTURES_REQUIRED engine_logs LABELS harness)
  endforeach()
  list(APPEND harness_tests harness_shadow harness_shadow_init harness_shadow_w1 harness_shadow_w1_init
    harness_shadow_w2 harness_shadow_w2_init harness_shadow_w3 harness_shadow_w3_init harness_engine)
  # Colosseum music overlay: trace the nickname, then a return-only cave, then PlayMusic
  foreach(stage trace return champion)
    add_test(NAME harness_music_${stage} COMMAND pkb_harness engine ${gb} --battles 1 --seed 1 --ai --levels 50-50
             --music ${stage})
    list(APPEND harness_tests harness_music_${stage})
  endforeach()
  # hardware battles replayed in the emulator
  if(PKB_REPLAY_LOGS)
    get_filename_component(logs ${PKB_REPLAY_LOGS} ABSOLUTE BASE_DIR ${PROJECT_SOURCE_DIR})
    file(GLOB replay_logs ${logs}/*.pbl)
    foreach(log ${replay_logs})
      get_filename_component(name ${log} NAME_WE)
      add_test(NAME harness_replay_${name} COMMAND pkb_harness replay ${log} ${gb})
      list(APPEND harness_tests harness_replay_${name})
    endforeach()
  endif()
  set_tests_properties(${harness_tests} PROPERTIES FIXTURES_REQUIRED colosseum LABELS harness TIMEOUT 1200)
endif()
