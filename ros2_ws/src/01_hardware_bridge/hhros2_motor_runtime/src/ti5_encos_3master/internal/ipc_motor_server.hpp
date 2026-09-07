#pragma once

#include "internal/ipc_motor_server_api.hpp"
#include "internal/motor_shared_state.hpp"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <rldf/ipc.h>
#include <qiuniu/init.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern void *server(void *);
