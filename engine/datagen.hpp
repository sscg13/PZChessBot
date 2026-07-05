/*
 * PZShatranjBot, a UCI shatranj engine derived from PZChessBot
 * Copyright (C) 2026 Kevin Lu and William Ma
 *
 * PZShatranjBot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 */

#pragma once

#include "includes.hpp"

int run_datagen(uint64_t target_positions, std::optional<uint64_t> seed, const std::string &output_file);
int run_datagen_workers(const std::string &executable, uint64_t positions_per_worker,
                        std::optional<uint64_t> seed, const std::string &output_file, size_t workers);
