#include "pymport.h"
#include "pystackobject.h"
#include "values.h"

using namespace Napi;
using namespace pymport;

Value PyObjectWrap::_Call(PyObject *py, const CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (!PyCallable_Check(py)) { throw Napi::TypeError::New(env, "Value not callable"); }

  PyStackObject kwargs = PyDict_New();
  size_t argc = info.Length();
  if (argc > 0 && info[argc - 1].IsObject() && !info[argc - 1].IsArray() && !_InstanceOf(info[argc - 1])) {
    PyObjectStore store;
    _Dictionary((info[argc - 1]).ToObject(), kwargs, store);
    argc--;
  }

  PyStackObject args = PyTuple_New(argc);
  for (size_t i = 0; i < argc; i++) {
    PyObject *v = FromJS(info[i]);
    THROW_IF_NULL(v);
    // FromJS returns a strong reference and PyTuple_SetItem steals it
    PyTuple_SetItem(args, i, v);
  }

  PyObject *r = PyObject_Call(py, args, kwargs);
  THROW_IF_NULL(r);

  return New(env, r);
}

Value PyObjectWrap::Call(const CallbackInfo &info) {
  return _Call(self, info);
}

Value PyObjectWrap::_CallableTrampoline(const CallbackInfo &info) {
  PyObject *py = reinterpret_cast<PyObject *>(info.Data());
  return _Call(py, info);
}

Value PyObjectWrap::Callable(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  return Boolean::New(env, PyCallable_Check(self));
}

Value PyObjectWrap::Eval(const CallbackInfo &info) {
  Napi::Env env = info.Env();
  auto text = NAPI_ARG_STRING(0).Utf8Value();
  PyStackObject globals = info.Length() > 1 ? FromJS(info[1]) : PyDict_New();
  PyStackObject locals = info.Length() > 2 ? FromJS(info[2]) : PyDict_New();

  PyObject *result = PyRun_String(text.c_str(), Py_eval_input, globals, locals);
  THROW_IF_NULL(result);

  return New(env, result);
}
