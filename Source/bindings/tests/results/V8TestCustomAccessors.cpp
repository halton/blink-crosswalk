/*
    This file is part of the Blink open source project.
    This file has been auto-generated by CodeGeneratorV8.pm. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include "V8TestCustomAccessors.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMConfiguration.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/Document.h"
#include "core/platform/chromium/TraceEvent.h"
#include "wtf/UnusedParam.h"

namespace WebCore {

static void initializeScriptWrappableForInterface(TestCustomAccessors* object)
{
    if (ScriptWrappable::wrapperCanBeStoredInObject(object))
        ScriptWrappable::setTypeInfoInObject(object, &V8TestCustomAccessors::info);
    else
        ASSERT_NOT_REACHED();
}

} // namespace WebCore

// In ScriptWrappable::init, the use of a local function declaration has an issue on Windows:
// the local declaration does not pick up the surrounding namespace. Therefore, we provide this function
// in the global namespace.
// (More info on the MSVC bug here: http://connect.microsoft.com/VisualStudio/feedback/details/664619/the-namespace-of-local-function-declarations-in-c)
void webCoreInitializeScriptWrappableForInterface(WebCore::TestCustomAccessors* object)
{
    WebCore::initializeScriptWrappableForInterface(object);
}

namespace WebCore {
WrapperTypeInfo V8TestCustomAccessors::info = { V8TestCustomAccessors::GetTemplate, V8TestCustomAccessors::derefObject, 0, 0, 0, V8TestCustomAccessors::installPerContextPrototypeProperties, 0, WrapperTypeObjectPrototype };

namespace TestCustomAccessorsV8Internal {

template <typename T> void V8_USE(T) { }

static void anotherFunctionMethod(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (UNLIKELY(args.Length() < 1)) {
        throwTypeError(ExceptionMessages::failedToExecute("anotherFunction", "TestCustomAccessors", ExceptionMessages::notEnoughArguments(1, args.Length())), args.GetIsolate());
        return;
    }
    TestCustomAccessors* imp = V8TestCustomAccessors::toNative(args.Holder());
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, str, args[0]);
    imp->anotherFunction(str);

    return;
}

static void anotherFunctionMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMMethod");
    TestCustomAccessorsV8Internal::anotherFunctionMethod(args);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

static void indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMIndexedProperty");
    V8TestCustomAccessors::indexedPropertyGetterCustom(index, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

static void indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMIndexedProperty");
    V8TestCustomAccessors::indexedPropertySetterCustom(index, value, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

static void indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMIndexedProperty");
    V8TestCustomAccessors::indexedPropertyDeleterCustom(index, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

static void namedPropertyGetterCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    V8TestCustomAccessors::namedPropertyGetterCustom(name, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

static void namedPropertySetterCallback(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    V8TestCustomAccessors::namedPropertySetterCustom(name, value, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

static void namedPropertyDeleterCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Boolean>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    V8TestCustomAccessors::namedPropertyDeleterCustom(name, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

static void namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    V8TestCustomAccessors::namedPropertyEnumeratorCustom(info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

static void namedPropertyQueryCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Integer>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMNamedProperty");
    V8TestCustomAccessors::namedPropertyQueryCustom(name, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}

} // namespace TestCustomAccessorsV8Internal

static const V8DOMConfiguration::MethodConfiguration V8TestCustomAccessorsMethods[] = {
    {"anotherFunction", TestCustomAccessorsV8Internal::anotherFunctionMethodCallback, 0, 1},
};

static v8::Handle<v8::FunctionTemplate> ConfigureV8TestCustomAccessorsTemplate(v8::Handle<v8::FunctionTemplate> desc, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    desc->ReadOnlyPrototype();

    v8::Local<v8::Signature> defaultSignature;
    defaultSignature = V8DOMConfiguration::installDOMClassTemplate(desc, "TestCustomAccessors", v8::Local<v8::FunctionTemplate>(), V8TestCustomAccessors::internalFieldCount,
        0, 0,
        V8TestCustomAccessorsMethods, WTF_ARRAY_LENGTH(V8TestCustomAccessorsMethods), isolate, currentWorldType);
    UNUSED_PARAM(defaultSignature);
    v8::Local<v8::ObjectTemplate> instance = desc->InstanceTemplate();
    v8::Local<v8::ObjectTemplate> proto = desc->PrototypeTemplate();
    UNUSED_PARAM(instance);
    UNUSED_PARAM(proto);
    desc->InstanceTemplate()->SetIndexedPropertyHandler(TestCustomAccessorsV8Internal::indexedPropertyGetterCallback, TestCustomAccessorsV8Internal::indexedPropertySetterCallback, 0, TestCustomAccessorsV8Internal::indexedPropertyDeleterCallback, 0);
    desc->InstanceTemplate()->SetNamedPropertyHandler(TestCustomAccessorsV8Internal::namedPropertyGetterCallback, TestCustomAccessorsV8Internal::namedPropertySetterCallback, TestCustomAccessorsV8Internal::namedPropertyQueryCallback, TestCustomAccessorsV8Internal::namedPropertyDeleterCallback, TestCustomAccessorsV8Internal::namedPropertyEnumeratorCallback);

    // Custom toString template
    desc->Set(v8::String::NewSymbol("toString"), V8PerIsolateData::current()->toStringTemplate());
    return desc;
}

v8::Handle<v8::FunctionTemplate> V8TestCustomAccessors::GetTemplate(v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    V8PerIsolateData::TemplateMap::iterator result = data->templateMap(currentWorldType).find(&info);
    if (result != data->templateMap(currentWorldType).end())
        return result->value.newLocal(isolate);

    TRACE_EVENT_SCOPED_SAMPLING_STATE("Blink", "BuildDOMTemplate");
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::FunctionTemplate> templ =
        ConfigureV8TestCustomAccessorsTemplate(data->rawTemplate(&info, currentWorldType), isolate, currentWorldType);
    data->templateMap(currentWorldType).add(&info, UnsafePersistent<v8::FunctionTemplate>(isolate, templ));
    return handleScope.Close(templ);
}

bool V8TestCustomAccessors::HasInstance(v8::Handle<v8::Value> value, v8::Isolate* isolate, WrapperWorldType currentWorldType)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&info, value, currentWorldType);
}

bool V8TestCustomAccessors::HasInstanceInAnyWorld(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    return V8PerIsolateData::from(isolate)->hasInstance(&info, value, MainWorld)
        || V8PerIsolateData::from(isolate)->hasInstance(&info, value, IsolatedWorld)
        || V8PerIsolateData::from(isolate)->hasInstance(&info, value, WorkerWorld);
}

v8::Handle<v8::Object> V8TestCustomAccessors::createWrapper(PassRefPtr<TestCustomAccessors> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl.get());
    ASSERT(!DOMDataStore::containsWrapper<V8TestCustomAccessors>(impl.get(), isolate));
    if (ScriptWrappable::wrapperCanBeStoredInObject(impl.get())) {
        const WrapperTypeInfo* actualInfo = ScriptWrappable::getTypeInfoFromObject(impl.get());
        // Might be a XXXConstructor::info instead of an XXX::info. These will both have
        // the same object de-ref functions, though, so use that as the basis of the check.
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(actualInfo->derefObjectFunction == info.derefObjectFunction);
    }

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &info, toInternalPointer(impl.get()), isolate);
    if (UNLIKELY(wrapper.IsEmpty()))
        return wrapper;

    installPerContextProperties(wrapper, impl.get(), isolate);
    V8DOMWrapper::associateObjectWithWrapper<V8TestCustomAccessors>(impl, &info, wrapper, isolate, WrapperConfiguration::Independent);
    return wrapper;
}

void V8TestCustomAccessors::derefObject(void* object)
{
    fromInternalPointer(object)->deref();
}

} // namespace WebCore
