/* Desktop Tq_Reader: stdio-backed, plus an in-memory variant. */
#pragma once
#include "../helpers/twentyq_dataset.h"

bool tq_test_reader_open(Tq_Reader* reader, const char* path);
void tq_test_reader_close(Tq_Reader* reader);
