#include "bindings.h"
#include "dataref.h"
#include "scenery.h"
#include "instance.h"
#include "graphics.h"
#include "skyscript.h"
#include "hid.h"
#include "log_msg.h"

void JSBindings::BindToView(RefPtr<View> view) {
    RefPtr<JSContext> context = view->LockJSContext();
    SetJSContext(context->ctx());
    JSObject global = JSGlobalObject();

    // Create the XPlane namespace object
    JSObject xplane;

    // DataRef sub-namespace
    JSObject dataref;
    DataRefBindings::Bind(dataref);
    xplane["dataref"] = JSValue(static_cast<JSObjectRef>(dataref));

    // Scenery sub-namespace
    JSObject scenery;
    SceneryBindings::Bind(scenery);
    xplane["scenery"] = JSValue(static_cast<JSObjectRef>(scenery));

    // Instance sub-namespace
    JSObject instance;
    InstanceBindings::Bind(instance);
    xplane["instance"] = JSValue(static_cast<JSObjectRef>(instance));

    // Graphics sub-namespace
    JSObject graphics;
    GraphicsBindings::Bind(graphics);
    xplane["graphics"] = JSValue(static_cast<JSObjectRef>(graphics));

    // HID sub-namespace
    JSObject hid;
    HidBindings::Bind(hid);
    xplane["hid"] = JSValue(static_cast<JSObjectRef>(hid));

    // Attach XPlane to global
    global["XPlane"] = JSValue(static_cast<JSObjectRef>(xplane));

    // SkyScript namespace (app management)
    JSObject skyscript;
    SkyScriptBindings::Bind(skyscript);
    global["SkyScript"] = JSValue(static_cast<JSObjectRef>(skyscript));

    LogMsg("JSBindings: Bound XPlane API (dataref, scenery, instance, graphics, hid) and SkyScript API to view");
}
