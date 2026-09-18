// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "debug.h"
#include "param.h"

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <mutex>
#include <pthread.h>

// NCCL's logger owns level/subsystem filtering. Do not access libnccl's
// private debug globals, or run its debug initialization inside the plugin.
int ncclDebugLevel = NCCL_LOG_TRACE;
uint64_t ncclDebugMask = NCCL_ALL;
FILE* ncclDebugFile = stdout;
thread_local int ncclDebugNoWarn = 0;

static std::atomic<ncclDebugLogger_t> pluginLogger{nullptr};

void ncclMrcSetLogger(ncclDebugLogger_t logFunction) {
  pluginLogger.store(logFunction, std::memory_order_release);
}

void ncclMrcLogInitOnce() {
  // NET v12 init has no rank argument. Use a launcher-provided GLOBAL rank,
  // never a local rank (which would print once on every node). Without a
  // known global rank, omit this optional banner rather than print on all ranks.
  static std::once_flag once;
  std::call_once(once, []() {
    static constexpr const char* rankVariables[] = {
      "OMPI_COMM_WORLD_RANK", "PMI_RANK", "PMIX_RANK", "RANK", "SLURM_PROCID"
    };
    const char* rank = nullptr;
    for (const char* name : rankVariables) {
      rank = ncclGetEnv(name);
      if (rank != nullptr) break;
    }
    if (rank == nullptr || *rank == '\0') return;
    char* end;
    errno = 0;
    long value = strtol(rank, &end, 10);
    if (errno != 0 || end == rank || *end != '\0' || value != 0) return;
    INFO(NCCL_INIT | NCCL_NET, "NET/MRC: Initializing rebased MRC plugin (v12)");
  });
}

static void pluginLog(ncclDebugLogLevel level, unsigned long flags, const char* filefunc, int line,
                      const char* fmt, va_list args) {
  ncclDebugLogger_t logger = pluginLogger.load(std::memory_order_acquire);
  if (logger == nullptr) return;
  if (level == NCCL_LOG_WARN && ncclDebugNoWarn) {
    level = NCCL_LOG_INFO;
    flags = ncclDebugNoWarn;
  }
  char message[4096];
  vsnprintf(message, sizeof(message), fmt, args);
  logger(level, flags, filefunc ? filefunc : "MRC", line, "%s", message);
}

void ncclDebugLogInternal(ncclDebugLogLevel level, unsigned long flags, const char* file, const char* func,
                          int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  pluginLog(level, flags, file ? file : func, line, fmt, args);
  va_end(args);
}

void ncclDebugLog(ncclDebugLogLevel level, unsigned long flags, const char* filefunc, int line,
                   const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  pluginLog(level, flags, filefunc, line, fmt, args);
  va_end(args);
}

NCCL_PARAM(SetThreadName, "SET_THREAD_NAME", 0);

void ncclSetThreadName(std::thread& thread, const char* fmt, ...) {
  if (!ncclParamSetThreadName()) return;
  char name[NCCL_THREAD_NAMELEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(name, sizeof(name), fmt, args);
  va_end(args);
  pthread_setname_np(thread.native_handle(), name);
}