#include <chrono>
#include <thread>
#include <mutex>
#include <node_api.h>
#include <memory>
#include <uv.h>
#include "addon.h"

#define READINTERVAL 1000

/*=============================================================*
* FUNCTION: I/O write
*==============================================================*/


/*=============================================================*
* FUNCTION: I/O read
*==============================================================*/
static bool g_reading(false);
static std::mutex g_reading_mutex;

static napi_ref g_read_ondata_callback_ref(NULL);
static std::mutex g_read_ondata_callback_ref_mutex;

static ReadOndataBaton *g_read_ondata_baton;
std::thread readThread_t;

napi_value onData(napi_env env, const napi_callback_info info){
  napi_status status;

  size_t argc = 1;
  napi_value args[1];
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  std::lock_guard<std::mutex> guard(g_read_ondata_callback_ref_mutex);
  if (g_read_ondata_callback_ref != NULL){
    status = napi_reference_unref(env, g_read_ondata_callback_ref, nullptr);
    status = napi_delete_reference(env, g_read_ondata_callback_ref);
    g_read_ondata_callback_ref = NULL;
  }
  if (argc != 0){
    status = napi_create_reference(env, args[0], 0, &g_read_ondata_callback_ref);
    status = napi_reference_ref(env, g_read_ondata_callback_ref, nullptr);
  }

  return nullptr;
}

// placeholder
napi_value onEnd(napi_env env, const napi_callback_info info){
  return nullptr;
}

napi_value readStart(napi_env env, const napi_callback_info info){
  {
    std::lock_guard<std::mutex> guard(g_reading_mutex);
    if (g_reading) return nullptr;
    g_reading = true;
  }

  g_read_ondata_baton = (ReadOndataBaton*) malloc(sizeof(ReadOndataBaton));
  g_read_ondata_baton->req.data = (void*) g_read_ondata_baton;
  g_read_ondata_baton->env = env;

  uv_async_init(uv_default_loop(), &g_read_ondata_baton->req, 
    [](uv_async_t* async) {
      std::lock_guard<std::mutex> guard(g_read_ondata_callback_ref_mutex);
      if (g_read_ondata_callback_ref != NULL){
        ReadOndataBaton* baton = static_cast<ReadOndataBaton*>(async->data);
        napi_env env = baton->env;
        napi_status status;

        napi_handle_scope scope;
        status = napi_open_handle_scope(env, &scope);
        size_t argc = 2;
        napi_value argv[2];
        status = napi_get_null(env, &argv[0]); // TBD: add error handling
        /* status = napi_create_uint32(env, (uint32_t)baton->device_buffer, &argv[1]); */
        size_t byte_length = 1;
        uint8_t** data(nullptr);
        status = napi_create_arraybuffer(env, byte_length, (void**)data, &argv[1]);
        *data[0] = baton->device_buffer;
            printf("afterRead: buffer = %d\n", baton->device_buffer);

        napi_value global;
        status = napi_get_global(env, &global);
        
        napi_value read_ondata_callback;
        status = napi_get_reference_value(env, g_read_ondata_callback_ref, &read_ondata_callback);
        status = napi_call_function(env, global, read_ondata_callback, argc, argv, nullptr);

        status = napi_close_handle_scope(env, scope);
      }
    }
  );

  readThread_t = std::thread(
    [](uv_async_t* async){
      bool reading;
      uint8_t fake_device_buffer(NULL);
      ReadOndataBaton* baton = static_cast<ReadOndataBaton*>(async->data);

      do {
        // emulate I/O device; low level I/O is here
        fake_device_buffer = 55;
        
        if (fake_device_buffer != NULL){
          std::lock_guard<std::mutex> guard(g_read_ondata_callback_ref_mutex);
          if (g_read_ondata_callback_ref != NULL){
            baton->device_buffer = fake_device_buffer;
            uv_async_send(async);
            printf("readThread: buffer = %d\n", baton->device_buffer);
          }
          fake_device_buffer = NULL;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(READINTERVAL));
        {
          std::lock_guard<std::mutex> guard(g_reading_mutex);
          reading = g_reading;
        }
      } while(reading);
    },
    &g_read_ondata_baton->req
  );

  return nullptr;
}

napi_value readStop(napi_env env, const napi_callback_info info){
  {
    std::lock_guard<std::mutex> guard(g_reading_mutex);
    if (!g_reading) return nullptr;
    g_reading = false;
  }

  readThread_t.join();

  uv_close((uv_handle_t*)&g_read_ondata_baton->req,
    [](uv_handle_t* handle) {
      free(g_read_ondata_baton);
    }
  );  

            printf("readStop: called %d\n", 0);
  return nullptr;
}

#define DECLARE_NAPI_METHOD(name, func) \
  {name, 0, func, 0, 0, 0, napi_default, 0}
  
napi_value Init(napi_env env, napi_value exports){
  napi_status status;
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_METHOD("readStart", readStart),
    DECLARE_NAPI_METHOD("readStop", readStop),
    DECLARE_NAPI_METHOD("onData", onData),
    DECLARE_NAPI_METHOD("onEnd", onEnd)
  };
  status = napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
  return exports;
}

NAPI_MODULE(addon, Init)