/*
 * Copyright (c) 2018
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <curl/curl.h>
#include <errno.h>

#include "metadata.h"
#include "edgex_rest.h"
#include "rest.h"
#include "errorlist.h"
#include "config.h"
#include "profiles.h"

static bool verifyInt (iot_logging_client *lc, const char *s, const char *f)
{
  if (s && *s)
  {
    char *end = NULL;
    errno = 0;
    strtoll (s, &end, 0);
    if (errno || (end && *end))
    {
      iot_log_error (lc, "Unable to parse %s as Integer for %s", s, f);
      return false;
    }
  }
  return true;
}

static bool verifyFloat (iot_logging_client *lc, const char *s, const char *f)
{
  if (s && *s)
  {
    char *end = NULL;
    errno = 0;
    strtold (s, &end);
    if (errno || (end && *end))
    {
      iot_log_error (lc, "Unable to parse %s as Float for %s", s, f);
      return false;
    }
  }
  return true;
}

/*
static bool verifyBlank (iot_logging_client *lc, const char *s, const char *f)
{
  if (s && strlen (s))
  {
    iot_log_error (lc, "Inapplicable %s specified in device resource", f);
    return false;
  }
  else
  {
    return true;
  }
}
*/

static bool verifyProfile (iot_logging_client *lc, edgex_deviceprofile *dp)
{
  edgex_device_resulttype rtype;
  edgex_propertyvalue *pv;

  for (edgex_deviceobject *res = dp->device_resources; res; res = res->next)
  {
    pv = res->properties->value;
    if (!edgex_string_to_resulttype (pv->type, &rtype))
    {
      iot_log_error
      (
        lc, "deviceResource %s has unknown value type %s", res->name, pv->type
      );
      return false;
    }
    switch (rtype)
    {
      case Uint8:
      case Uint16:
      case Uint32:
      case Uint64:
      case Int8:
      case Int16:
      case Int32:
      case Int64:
/* TODO: These should all be verifyInt, but current metadata implementation
   sets "1.0" for default scale and "0.0" for default offset */

        if (!verifyFloat (lc, pv->offset, "offset") ||
            !verifyFloat (lc, pv->scale, "scale") ||
            !verifyInt (lc, pv->base, "base"))
        {
          iot_log_error
            (lc, "Invalid transform in deviceResource %s", res->name);
          return false;
        }
        break;
      case Float32:
      case Float64:
        if (!verifyFloat (lc, pv->offset, "offset") ||
            !verifyFloat (lc, pv->scale, "scale") ||
            !verifyFloat (lc, pv->base, "base"))
        {
          iot_log_error
            (lc, "Invalid transform in deviceResource %s", res->name);
          return false;
        }
        break;
      default:
/* TODO: metadata sets default values for these. Enable these tests when fixed.

        if (!verifyBlank (lc, pv->offset, "offset") ||
            !verifyBlank (lc, pv->scale, "scale") ||
            !verifyBlank (lc, pv->base, "base"))
        {
          iot_log_error
            (lc, "Invalid transform in deviceResource %s", res->name);
          return false;
        }
*/ ;
    }
  }
  return true;
}

edgex_deviceprofile *edgex_metadata_client_get_deviceprofile
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  edgex_error *err
)
{
  edgex_ctx ctx;
  edgex_deviceprofile *result = 0;
  char *ename;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  ename = curl_easy_escape (NULL, name, 0);

  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/deviceprofile/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    ename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    result = 0;
  }
  else
  {
    result = edgex_deviceprofile_read (ctx.buff);
    if (verifyProfile (lc, result))
    {
      *err = EDGEX_OK;
    }
    else
    {
      *err = EDGEX_PROFILE_PARSE_ERROR;
      iot_log_error (lc, "Parse error reading device profile %s", name);
      edgex_deviceprofile_free (result);
      result = 0;
    }
  }
  curl_free (ename);
  free (ctx.buff);
  return result;
}

void edgex_metadata_client_set_device_opstate
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *deviceid,
  bool enabled,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/%s/opstate/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    deviceid,
    enabled ? "enabled" : "disabled"
  );

  edgex_http_put (lc, &ctx, url, NULL, edgex_http_write_cb, err);
  free (ctx.buff);
}

void edgex_metadata_client_set_device_adminstate
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *deviceid,
  bool locked,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/%s/adminstate/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    deviceid,
    locked ? "LOCKED" : "UNLOCKED"
  );

  edgex_http_put (lc, &ctx, url, NULL, edgex_http_write_cb, err);
  free (ctx.buff);
}

char *edgex_metadata_client_create_deviceprofile
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const edgex_deviceprofile *newdp,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/deviceprofile",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_deviceprofile_write (newdp, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  free (json);
  return ctx.buff;
}

char *edgex_metadata_client_create_deviceprofile_file
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *filename,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/deviceprofile/uploadfile",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  edgex_http_postfile (lc, &ctx, url, filename, edgex_http_write_cb, err);
  return ctx.buff;
}

edgex_deviceservice *edgex_metadata_client_get_deviceservice
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  edgex_error *err
)
{
  edgex_ctx ctx;
  edgex_deviceservice *result = 0;
  char url[URL_BUF_SIZE];
  long rc;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/deviceservice/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    name
  );

  rc = edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (rc == 404)
  {
    *err = EDGEX_OK;
    free (ctx.buff);
    return 0;
  }

  if (err->code)
  {
    return 0;
  }

  result = edgex_deviceservice_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

char *edgex_metadata_client_create_deviceservice
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const edgex_deviceservice *newds,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/deviceservice",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_deviceservice_write (newds, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  free (json);
  return ctx.buff;
}

edgex_device *edgex_metadata_client_get_devices
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *servicename,
  edgex_error *err
)
{
  edgex_ctx ctx;
  edgex_device *result = 0;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/servicename/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    servicename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    return 0;
  }

  result = edgex_devices_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

edgex_scheduleevent *edgex_metadata_client_get_scheduleevents
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *servicename,
  edgex_error *err
)
{
  edgex_ctx ctx;
  edgex_scheduleevent *result = 0;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/scheduleevent/servicename/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    servicename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    return 0;
  }

  result = edgex_scheduleevents_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

edgex_scheduleevent *edgex_metadata_client_create_scheduleevent
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  uint64_t origin,
  const char *schedule_name,
  const char *addressable_name,
  const char *parameters,
  const char *service_name,
  edgex_error *err
)
{
  edgex_scheduleevent *result = malloc (sizeof (edgex_scheduleevent));
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (result, 0, sizeof (edgex_scheduleevent));
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/scheduleevent",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  result->name = strdup (name);
  result->origin = origin;
  result->schedule = strdup (schedule_name);
  result->addressable = malloc (sizeof (edgex_addressable));
  memset (result->addressable, 0, sizeof (edgex_addressable));
  result->addressable->name = strdup (addressable_name);
  result->parameters = strdup (parameters);
  result->service = strdup (service_name);
  json = edgex_scheduleevent_write (result, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code == 0)
  {
    result->id = ctx.buff;
  }
  else
  {
    iot_log_info
    (
      lc,
      "edgex_metadata_client_create_scheduleevent: %s: %s",
      err->reason,
      ctx.buff
    );
    free (ctx.buff);
  }
  free (json);

  return result;
}

edgex_schedule *edgex_metadata_client_get_schedule
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *schedulename,
  edgex_error *err
)
{
  edgex_ctx ctx;
  edgex_schedule *result = 0;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/schedule/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    schedulename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    return 0;
  }

  result = edgex_schedule_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

edgex_schedule *edgex_metadata_client_create_schedule
  (
    iot_logging_client *lc,
    edgex_service_endpoints *endpoints,
    const char *name,
    uint64_t origin,
    const char *frequency,
    const char *start,
    const char *end,
    bool runOnce,
    edgex_error *err
  )
{
  edgex_schedule *result = malloc (sizeof (edgex_schedule));
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (result, 0, sizeof (edgex_schedule));
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/schedule",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  result->name = strdup (name);
  result->origin = origin;
  result->frequency = strdup (frequency);
  result->start = strdup (start);
  result->end = strdup (end);
  result->runOnce = runOnce;
  json = edgex_schedule_write (result, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code == 0)
  {
    result->id = ctx.buff;
  }
  else
  {
    iot_log_info
    (
      lc,
      "edgex_metadata_client_create_schedule: %s: %s",
      err->reason,
      ctx.buff
    );
    free (ctx.buff);
  }
  free (json);

  return result;
}

edgex_device *edgex_metadata_client_add_device
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  const char *description,
  const edgex_strings *labels,
  uint64_t origin,
  const char *addressable_name,
  const char *service_name,
  const char *profile_name,
  edgex_error *err
)
{
  edgex_device *result = malloc (sizeof (edgex_device));
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (result, 0, sizeof (edgex_device));
  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  result->name = strdup (name);
  result->description = strdup (description);
  result->adminState = strdup ("UNLOCKED");
  result->operatingState = strdup ("ENABLED");
  result->labels = edgex_strings_dup (labels);
  result->addressable = malloc (sizeof (edgex_addressable));
  memset (result->addressable, 0, sizeof (edgex_addressable));
  result->addressable->name = strdup (addressable_name);
  result->service = malloc (sizeof (edgex_deviceservice));
  memset (result->service, 0, sizeof (edgex_deviceservice));
  result->service->name = strdup (service_name);
  result->profile = malloc (sizeof (edgex_deviceprofile));
  memset (result->profile, 0, sizeof (edgex_deviceprofile));
  result->profile->name = strdup (profile_name);
  json = edgex_device_write (result, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code == 0)
  {
    result->id = ctx.buff;
  }
  else
  {
    iot_log_info
    (
      lc,
      "edgex_metadata_client_add_device: %s: %s",
      err->reason,
      ctx.buff
    );
    free (ctx.buff);
  }
  free (json);

  return result;
}

edgex_device *edgex_metadata_client_get_device
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *deviceid,
  edgex_error *err
)
{
  edgex_ctx ctx;
  edgex_device *result = 0;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    deviceid
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    return 0;
  }

  result = edgex_device_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

edgex_device *edgex_metadata_client_get_device_byname
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *devicename,
  edgex_error *err
)
{
  edgex_ctx ctx;
  edgex_device *result = 0;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    devicename
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    return 0;
  }

  result = edgex_device_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

void edgex_metadata_client_update_device
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  const char * id,
  const char * description,
  const edgex_strings * labels,
  const char * profile_name,
  edgex_error * err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device",
    endpoints->metadata.host,
    endpoints->metadata.port
  );

  json = edgex_device_write_sparse
    (name, id, description, labels, profile_name);

  edgex_http_put (lc, &ctx, url, json, edgex_http_write_cb, err);
  if (err->code != 0)
  {
    iot_log_info
    (
      lc,
      "edgex_metadata_client_update_device: %s: %s",
      err->reason,
      ctx.buff
    );
  }
  free (ctx.buff);
  free (json);
}

void edgex_metadata_client_delete_device
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * deviceid,
  edgex_error * err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/id/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    deviceid
  );

  edgex_http_delete (lc, &ctx, url, edgex_http_write_cb, err);

  free (ctx.buff);
}

void edgex_metadata_client_delete_device_byname
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * devicename,
  edgex_error * err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/device/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    devicename
  );

  edgex_http_delete (lc, &ctx, url, edgex_http_write_cb, err);

  free (ctx.buff);
}

edgex_addressable *edgex_metadata_client_get_addressable
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const char *name,
  edgex_error *err
)
{
  edgex_ctx ctx;
  edgex_addressable *result = 0;
  char url[URL_BUF_SIZE];
  long rc;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/addressable/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    name
  );

  rc = edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  if (err->code)
  {
    if (rc == 404)
    {
      *err = EDGEX_OK;
    }
    free (ctx.buff);
    return 0;
  }

  result = edgex_addressable_read (ctx.buff);
  free (ctx.buff);
  *err = EDGEX_OK;
  return result;
}

char *edgex_metadata_client_create_addressable
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const edgex_addressable *newadd,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/addressable",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_addressable_write (newadd, true);
  edgex_http_post (lc, &ctx, url, json, edgex_http_write_cb, err);
  free (json);
  return ctx.buff;
}

void edgex_metadata_client_update_addressable
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  const edgex_addressable *addressable,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];
  char *json;

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/addressable",
    endpoints->metadata.host,
    endpoints->metadata.port
  );
  json = edgex_addressable_write (addressable, false);
  edgex_http_put (lc, &ctx, url, json, edgex_http_write_cb, err);
  free (json);
  free (ctx.buff);
}

void edgex_metadata_client_delete_addressable
(
  iot_logging_client * lc,
  edgex_service_endpoints * endpoints,
  const char * name,
  edgex_error * err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/addressable/name/%s",
    endpoints->metadata.host,
    endpoints->metadata.port,
    name
  );

  edgex_http_delete (lc, &ctx, url, edgex_http_write_cb, err);

  free (ctx.buff);
}

bool edgex_metadata_client_ping
(
  iot_logging_client *lc,
  edgex_service_endpoints *endpoints,
  edgex_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  snprintf
  (
    url,
    URL_BUF_SIZE - 1,
    "http://%s:%u/api/v1/ping",
    endpoints->metadata.host,
    endpoints->metadata.port
  );

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);
  free (ctx.buff);

  return (err->code == 0);
}
