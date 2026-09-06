string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef RUN_ID)
set(DIR "${OUTPUT_DIR}/${RUN_ID}")
execute_process(
    COMMAND "${RUNNER}" --synthetic --blocks 127,1,511
        --output "${DIR}/synthetic.wav" --report "${DIR}/synthetic.csv"
    RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Synthetic bench failed: ${result}")
endif()
