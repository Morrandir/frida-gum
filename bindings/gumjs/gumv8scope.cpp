/*
 * Copyright (C) 2015-2016 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8scope.h"

#include "gumv8script-priv.h"

using namespace v8;

ScriptScope::ScriptScope (GumV8Script * parent)
  : parent (parent),
    stalker_scope (parent),
    locker (parent->priv->isolate),
    isolate_scope (parent->priv->isolate),
    handle_scope (parent->priv->isolate),
    context (Local<Context>::New (parent->priv->isolate, *parent->priv->context)),
    context_scope (context),
    trycatch (parent->priv->isolate),
    interceptor_scope (parent)
{
  _gum_v8_core_pin (&parent->priv->core);
}

ScriptScope::~ScriptScope ()
{
  GumV8ScriptPrivate * priv = parent->priv;
  GumV8Core * core = &priv->core;

  if (trycatch.HasCaught ())
  {
    Handle<Value> exception = trycatch.Exception ();
    trycatch.Reset ();
    _gum_v8_core_on_unhandled_exception (&priv->core, exception);
    trycatch.Reset ();
  }

  if (!g_queue_is_empty (core->tick_callbacks))
  {
    Isolate * isolate = parent->priv->isolate;

    GumPersistent<Function>::type * tick_callback;
    Local<Value> null_value = Null (isolate);
    while ((tick_callback = static_cast<GumPersistent<Function>::type *> (
        g_queue_pop_head (core->tick_callbacks))) != nullptr)
    {
      Local<Function> callback (Local<Function>::New (isolate, *tick_callback));

      callback->Call (null_value, 0, nullptr);
      if (trycatch.HasCaught ())
      {
        Handle<Value> exception = trycatch.Exception ();
        trycatch.Reset ();
        _gum_v8_core_on_unhandled_exception (&priv->core, exception);
        trycatch.Reset ();
      }

      delete tick_callback;
    }
  }

  _gum_v8_core_unpin (core);

  GumV8FlushNotify pending_flush_notify = core->flush_notify;
  if (pending_flush_notify != NULL && core->usage_count == 0)
  {
    core->flush_notify = NULL;

    Isolate * isolate = parent->priv->isolate;
    isolate->Exit ();
    {
      Unlocker ul (isolate);

      _gum_v8_core_notify_flushed (core, pending_flush_notify);
    }
    isolate->Enter ();
  }
}

ScriptInterceptorScope::ScriptInterceptorScope (GumV8Script * parent)
  : parent (parent)
{
  gum_interceptor_begin_transaction (parent->priv->interceptor.interceptor);
}

ScriptInterceptorScope::~ScriptInterceptorScope ()
{
  gum_interceptor_end_transaction (parent->priv->interceptor.interceptor);
}

ScriptStalkerScope::ScriptStalkerScope (GumV8Script * parent)
  : parent (parent)
{
}

ScriptStalkerScope::~ScriptStalkerScope ()
{
  _gum_v8_stalker_process_pending (&parent->priv->stalker);
}
