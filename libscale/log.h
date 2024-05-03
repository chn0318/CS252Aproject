// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#define LOG_LEVEL 0
/*
#define LOG_INFO(x) (((LOG_LEVEL) <= 3) ? std::cout << "[INFO] "  << x << std::endl : std::cout << "")
#define LOG_DEBUG(x) (((LOG_LEVEL) <= 2) ? std::cout << "[DEBUG] " << x << std::endl : std::cout << "")
#define LOG_TRACE(x) (((LOG_LEVEL) <= 1) ? std::cout << "[TRACE] " << x << std::endl : std::cout << "")
#define LOG_ERROR(x) (std::cerr << "[ERROR] " << x << std::endl);
*/
#define LOG_INFO_PRINTF printf("[INFO] "); if (LOG_LEVEL <=3) printf
#define LOG_DEBUG_PRINTF printf("[DEBUG] "); if (LOG_LEVEL <=2) printf
#define LOG_TRACE_PRINTF printf("[TRACE] "); if (LOG_LEVEL <=1) printf
#define LOG_ERROR_PRINTF printf("[ERROR] "); printf

#endif /* LOG_H */
