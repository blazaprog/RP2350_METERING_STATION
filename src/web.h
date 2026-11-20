#pragma once
#include <stddef.h>
#include "sensor_data.h"

// Builds a full HTTP response for the request. Returns length written to resp.
size_t http_handle(const char *req, char *resp, size_t cap, const sensor_data_t *d);
