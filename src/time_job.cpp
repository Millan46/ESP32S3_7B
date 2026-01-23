#include "time_job.h"

volatile bool g_time_save_pending = false;
PendingTime g_pending_time = {0};
