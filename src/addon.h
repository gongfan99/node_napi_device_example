typedef struct {
  uv_async_t req;
  napi_env env;
  uint8_t device_buffer;
} ReadOndataBaton;