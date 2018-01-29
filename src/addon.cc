#include <chrono>
#include <thread>
#include <mutex>
#include <node_api.h>
#include <memory>
#include <uv.h>
#include "addon.h"

#define READINTERVAL 1000

static bool g_reading(false);
static std::mutex g_reading_mutex;

static napi_ref g_read_callback_ref(NULL);
static std::mutex g_read_callback_ref_mutex;

static ReadBaton *g_baton;
std::thread readThread_t;

napi_value onData(napi_env env, const napi_callback_info info){
  printf("onData: uv loop handles: %d\n", uv_loop_alive(uv_default_loop()));
  napi_status status;

  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  if (argc == 0){
    std::lock_guard<std::mutex> guard(g_read_callback_ref_mutex);
    if (g_read_callback_ref != NULL){
      status = napi_reference_unref(env, g_read_callback_ref, nullptr);
      status = napi_delete_reference(env, g_read_callback_ref);
      g_read_callback_ref = NULL;
    }
    return nullptr;
  }

  {
    std::lock_guard<std::mutex> guard(g_read_callback_ref_mutex);
    status = napi_create_reference(env, args[0], 0, &g_read_callback_ref);
    status = napi_reference_ref(env, g_read_callback_ref, nullptr);
  }

  return nullptr;
}

void readThread(uv_async_t* async){
  bool reading;
  uint8_t fake_device_buffer(NULL);
  ReadBaton* baton = static_cast<ReadBaton*>(async->data);
  printf("readThread: I am running\n");

  do {
    // emulate I/O device
    fake_device_buffer = 55;
    printf("readThread: fake_device_buffer = %d\n", fake_device_buffer);
    printf("readThread: I get mutex\n");
    
    if (fake_device_buffer != NULL){
      std::lock_guard<std::mutex> guard(g_read_callback_ref_mutex);
      if (g_read_callback_ref != NULL){
        baton->device_buffer = fake_device_buffer;
        uv_async_send(async);
        printf("readThread: uv_async_send data is %d\n", baton->device_buffer);
      }
      fake_device_buffer = NULL;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(READINTERVAL));
    {
      std::lock_guard<std::mutex> guard(g_reading_mutex);
      reading = g_reading;
    }
    printf("readThread: reading = %s\n", reading ? "true" : "false");
  } while(reading);
  printf("readThread: I am quiting\n");
}

void afterRead(uv_async_t* async) {
  printf("afterRead: uv_async_send data is %d\n", static_cast<ReadBaton*>(async->data)->device_buffer);
  printf("afterRead: g_read_callback is null? %s\n", g_read_callback_ref == NULL ? "true" : "false");
  std::lock_guard<std::mutex> guard(g_read_callback_ref_mutex);
  if (g_read_callback_ref != NULL){
    ReadBaton* baton = static_cast<ReadBaton*>(async->data);
    napi_env env = baton->env;
    napi_status status;

    napi_handle_scope scope;
    status = napi_open_handle_scope(env, &scope);
    size_t argc = 2;
    napi_value argv[2];
    status = napi_get_null(env, &argv[0]); // TBD: add error handling
    printf("afterRead: status is %d\n", status);
    printf("afterRead: baton->device_buffer = %d\n", baton->device_buffer);
    status = napi_create_uint32(env, (uint32_t)baton->device_buffer, &argv[1]);
    printf("afterRead: status is %d\n", status);
    printf("afterRead: napi_ok is %d\n", napi_ok);
    if (status != napi_ok) printf("afterRead: status is %d\n", status);

    napi_value global;
    status = napi_get_global(env, &global);
    
    napi_value read_callback;
    status = napi_get_reference_value(env, g_read_callback_ref, &read_callback);
    status = napi_call_function(env, global, read_callback, argc, argv, nullptr);
    printf("afterRead: read_callback is called\n");

    status = napi_close_handle_scope(env, scope);
  }
  printf("afterRead: return\n");
}

napi_value readStart(napi_env env, const napi_callback_info info){
  printf("readStart: uv loop handles: %d\n", uv_loop_alive(uv_default_loop()));

  {
    std::lock_guard<std::mutex> guard(g_reading_mutex);
    if (g_reading) return nullptr;
    g_reading = true;
  }

  g_baton = (ReadBaton*) malloc(sizeof(ReadBaton));
  g_baton->req.data = (void*) g_baton;
  g_baton->env = env;
  uv_async_init(uv_default_loop(), &g_baton->req, afterRead);
  readThread_t = std::thread(readThread, &g_baton->req);

  return nullptr;
}

napi_value readStop(napi_env env, const napi_callback_info info){
  /* napi_status status; */

  {
    std::lock_guard<std::mutex> guard(g_reading_mutex);
    if (!g_reading) return nullptr;
    g_reading = false;
  }

  readThread_t.join();

  uv_close((uv_handle_t*)&g_baton->req,
    [](uv_handle_t* handle) {
      printf("readStop: handle alive? %d\n", uv_is_active(handle));
      printf("readStop: handle closing? %d\n", uv_is_closing(handle));
      free(g_baton);
      printf("readStop: I am called. uv loop handles: %d\n", uv_loop_alive(uv_default_loop()));
    }
  );  

  return nullptr;
}

#define DECLARE_NAPI_METHOD(name, func) \
  {name, 0, func, 0, 0, 0, napi_default, 0}
  
napi_value Init(napi_env env, napi_value exports){
  napi_status status;
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_METHOD("readStart", readStart),
    DECLARE_NAPI_METHOD("readStop", readStop),
    DECLARE_NAPI_METHOD("onData", onData)
  };
  status = napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
  return exports;
}

NAPI_MODULE(addon, Init)