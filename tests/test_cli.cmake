# Each invocation gets its own directory; tests never delete a user's audio.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef RUN_ID)
set(DIR "${OUTPUT_DIR}/${RUN_ID}")
file(MAKE_DIRECTORY "${DIR}")

function(run_ok)
    execute_process(COMMAND "${RUNNER}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Runner failed: ${ARGN}\n${output}\n${error}")
    endif()
endfunction()

function(run_bad)
    execute_process(COMMAND "${RUNNER}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result STREQUAL "1")
        message(FATAL_ERROR "Expected controlled rejection: ${ARGN}\n${result}\n${error}")
    endif()
endfunction()

run_ok(--synthetic --channels 2 --sample-rate 44100 --blocks 127,1,511
    --output "${DIR}/a.wav" --report "${DIR}/a.csv")
run_ok(--synthetic --channels 2 --sample-rate 44100 --blocks 256
    --output "${DIR}/b.wav" --report "${DIR}/b.csv")
run_ok(--input "${DIR}/a.wav" --blocks 1,2048,7
    --output "${DIR}/roundtrip.wav" --report "${DIR}/roundtrip.csv")

file(SHA256 "${DIR}/a.wav" original_hash)
foreach(name b roundtrip)
    file(SHA256 "${DIR}/${name}.wav" result_hash)
    if(NOT original_hash STREQUAL result_hash)
        message(FATAL_ERROR "Float WAV round trip or block invariance failed: ${name}")
    endif()
endforeach()
file(READ "${DIR}/a.csv" a)
file(READ "${DIR}/b.csv" b)
if(NOT a STREQUAL b)
    message(FATAL_ERROR "Reports are not deterministic across block sizes")
endif()
string(REGEX MATCHALL "known_hit" hits "${a}")
list(LENGTH hits count)
if(NOT count EQUAL 8)
    message(FATAL_ERROR "Expected four ground-truth hits per channel")
endif()

run_bad(--synthetic --blocks 0 --output "${DIR}/bad.wav" --report "${DIR}/bad.csv")
run_bad(--synthetic --blocks 1, --output "${DIR}/bad.wav" --report "${DIR}/bad.csv")
run_bad(--synthetic --channels 3 --output "${DIR}/bad.wav" --report "${DIR}/bad.csv")
run_bad(--synthetic --sample-rate 999999 --output "${DIR}/bad.wav" --report "${DIR}/bad.csv")
run_bad(--input "${DIR}/missing.wav" --output "${DIR}/bad.wav" --report "${DIR}/bad.csv")
run_bad(--synthetic --input "${DIR}/a.wav" --output "${DIR}/bad.wav" --report "${DIR}/bad.csv")
run_bad(--synthetic --output "${DIR}/same" --report "${DIR}/same")
run_bad(--input "${DIR}/a.wav" --output "${DIR}/a.wav" --report "${DIR}/bad.csv")
run_bad(--synthetic --output "${DIR}/bad.wav" --report "${DIR}/a.csv")
file(SHA256 "${DIR}/a.wav" final_hash)
if(NOT original_hash STREQUAL final_hash)
    message(FATAL_ERROR "Input file was overwritten")
endif()
