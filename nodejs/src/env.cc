#include "env.h"
#include "env-inl.h"
#include "v8.h"

#if defined(_MSC_VER)
#define getpid GetCurrentProcessId
#else
#include <unistd.h>
#endif

#include <stdio.h>

namespace node {

using v8::HandleScope;
using v8::Local;
using v8::Message;
using v8::StackFrame;
using v8::StackTrace;
using v8::TryCatch;
using v8::Value;

void Environment::PrintSyncTrace() const {
  if (!trace_sync_io_)
    return;

  HandleScope handle_scope(isolate());
  Local<v8::StackTrace> stack =
      StackTrace::CurrentStackTrace(isolate(), 10, StackTrace::kDetailed);

  fprintf(stderr, "WARNING: Detected use of sync API\n");

  for (int i = 0; i < stack->GetFrameCount() - 1; i++) {
    Local<StackFrame> stack_frame = stack->GetFrame(i);
    node::Utf8Value fn_name_s(isolate(), stack_frame->GetFunctionName());
    node::Utf8Value script_name(isolate(), stack_frame->GetScriptName());
    const int line_number = stack_frame->GetLineNumber();
    const int column = stack_frame->GetColumn();

    if (stack_frame->IsEval()) {
      if (stack_frame->GetScriptId() == Message::kNoScriptIdInfo) {
        fprintf(stderr, "    at [eval]:%i:%i\n", line_number, column);
      } else {
        fprintf(stderr,
                "    at [eval] (%s:%i:%i)\n",
                *script_name,
                line_number,
                column);
      }
      break;
    }

    if (fn_name_s.length() == 0) {
      fprintf(stderr, "    at %s:%i:%i\n", *script_name, line_number, column);
    } else {
      fprintf(stderr,
              "    at %s (%s:%i:%i)\n",
              *fn_name_s,
              *script_name,
              line_number,
              column);
    }
  }
  fflush(stderr);
}


bool Environment::KickNextTick(Environment::AsyncCallbackScope* scope) {
  TickInfo* info = tick_info();

  if (scope->in_makecallback()) {
    return true;
  }

  if (info->length() == 0) {
    isolate()->RunMicrotasks();
  }

  if (info->length() == 0) {
    info->set_index(0);
    return true;
  }

  Local<Value> ret =
    tick_callback_function()->Call(process_object(), 0, nullptr);

  return !ret.IsEmpty();
}

}  // namespace node
