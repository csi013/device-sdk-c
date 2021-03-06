/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_DEVICE_PROFILES_H_
#define _EDGEX_DEVICE_PROFILES_H_ 1

#include "service.h"

extern void edgex_device_profiles_upload
(
  edgex_device_service *svc,
  edgex_error *err
);

edgex_deviceprofile *edgex_deviceprofile_get
(
  edgex_device_service *svc,
  const char *name,
  edgex_error *err
);

bool edgex_string_to_resulttype (const char *str, edgex_device_resulttype *res);

#endif
