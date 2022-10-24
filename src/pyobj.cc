#include "pymport.h"
#include "pystackobject.h"
#include "values.h"

using namespace Napi;
using namespace pymport;

PyObjectWrap::PyObjectWrap(const CallbackInfo &info) : ObjectWrap(info) {
  Napi::Env env = info.Env();

  if (info.Length() < 1) throw TypeError::New(env, "Cannot create an empty object");

  if (info[0].IsExternal()) {
    self = info[0].As<External<PyObject>>().Data();
  } else {
    // Reference unicity cannot be achieved with a constructor
    throw Error::New(env, "Use PyObject.fromJS() to create PyObjects");
  }
}

PyObjectWrap::~PyObjectWrap() {
  // self == nullptr when the object has been evicted from the ObjectStore
  // because it was dying - refer to the comments there
  if (self != nullptr) {
    Release();
    Py_DECREF(self);
  }
}

Function PyObjectWrap::GetClass(Napi::Env env) {
  return DefineClass(
    env,
    "PyObject",
    {PyObjectWrap::InstanceMethod("toString", &PyObjectWrap::ToString),
     PyObjectWrap::InstanceMethod("get", &PyObjectWrap::Get),
     PyObjectWrap::InstanceMethod("has", &PyObjectWrap::Has),
     PyObjectWrap::InstanceMethod("item", &PyObjectWrap::Item),
     PyObjectWrap::InstanceMethod("call", &PyObjectWrap::Call),
     PyObjectWrap::InstanceMethod("toJS", &PyObjectWrap::ToJS),
     PyObjectWrap::InstanceAccessor("type", &PyObjectWrap::Type, nullptr),
     PyObjectWrap::InstanceAccessor("callable", &PyObjectWrap::Callable, nullptr),
     PyObjectWrap::InstanceAccessor("length", &PyObjectWrap::Length, nullptr),
     PyObjectWrap::StaticMethod("fromJS", &PyObjectWrap::FromJS),
     PyObjectWrap::StaticMethod("string", &PyObjectWrap::String),
     PyObjectWrap::StaticMethod("int", &PyObjectWrap::Integer),
     PyObjectWrap::StaticMethod("float", &PyObjectWrap::Float),
     PyObjectWrap::StaticMethod("dict", &PyObjectWrap::Dictionary),
     PyObjectWrap::StaticMethod("list", &PyObjectWrap::List),
     PyObjectWrap::StaticMethod("tuple", &PyObjectWrap::Tuple),
     PyObjectWrap::StaticMethod("slice", &PyObjectWrap::Slice)});
}

Value PyObjectWrap::ToString(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  PyStackObject r = PyObject_Str(self);
  THROW_IF_NULL(r);
  return ToJS(env, r);
}

Value PyObjectWrap::Get(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  std::string name = NAPI_ARG_STRING(0).Utf8Value();
  auto r = PyObject_GetAttrString(self, name.c_str());
  return New(env, r);
}

Value PyObjectWrap::Import(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  std::string name = NAPI_ARG_STRING(0).Utf8Value();
  PyStackObject pyname = PyUnicode_DecodeFSDefault(name.c_str());
  THROW_IF_NULL(pyname);

  auto obj = PyImport_Import(pyname);
  return New(env, obj);
}

Value PyObjectWrap::Has(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  std::string name = NAPI_ARG_STRING(0).Utf8Value();
  auto r = PyObject_HasAttrString(self, name.c_str());
  return Boolean::New(env, r);
}

Value PyObjectWrap::Type(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  return String::New(env, self->ob_type->tp_name);
}

Value PyObjectWrap::Item(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (PyList_Check(self)) {
    Py_ssize_t idx = NAPI_ARG_NUMBER(0).Int64Value();
    auto r = PyList_GetItem(self, idx);
    THROW_IF_NULL(r);
    // PyList returns a borrowed reference, New expects a strong one
    Py_INCREF(r);
    return New(env, r);
  }
  if (PyTuple_Check(self)) {
    Py_ssize_t idx = NAPI_ARG_NUMBER(0).Int64Value();
    auto r = PyTuple_GetItem(self, idx);
    THROW_IF_NULL(r);
    if (r == nullptr) return env.Undefined();
    // PyTuple returns a borrowed reference, New expects a strong one
    Py_INCREF(r);
    return New(env, r);
  }
  PyStackObject getitem = PyObject_GetAttrString(self, "__getitem__");
  if ((PyObject *)getitem != nullptr) { return _Call(getitem, info); }

  return env.Undefined();
}

Value PyObjectWrap::Length(const CallbackInfo &info) {
  Napi::Env env = info.Env();

  if (PyList_Check(self)) { return Number::New(env, static_cast<long>(PyList_Size(self))); }
  if (PyTuple_Check(self)) return Number::New(env, static_cast<long>(PyTuple_Size(self)));
  if (PyDict_Check(self)) return Number::New(env, static_cast<long>(PyDict_Size(self)));
  if (PyUnicode_Check(self)) return Number::New(env, static_cast<long>(PyUnicode_GetLength(self)));
  return env.Undefined();
}

bool PyObjectWrap::_InstanceOf(Napi::Value v) {
  Napi::Env env = v.Env();
  if (!v.IsObject()) return false;
  auto obj = v.ToObject();
  FunctionReference *cons = env.GetInstanceData<EnvContext>()->pyObj;
  return obj.ToObject().InstanceOf(cons->Value());
}

bool PyObjectWrap::_FunctionOf(Napi::Value v) {
  Napi::Env env = v.Env();
  if (!v.IsObject()) return false;
  auto obj = v.ToObject().Get("__PyObject__");
  if (!obj.IsObject()) return false;
  FunctionReference *cons = env.GetInstanceData<EnvContext>()->pyObj;
  return obj.ToObject().InstanceOf(cons->Value());
}