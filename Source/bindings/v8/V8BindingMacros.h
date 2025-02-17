/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8BindingMacros_h
#define V8BindingMacros_h

namespace WebCore {

#define V8TRYCATCH(type, var, value) \
    type var;                             \
    {                                     \
        v8::TryCatch block;               \
        var = (value);                    \
        if (block.HasCaught())            \
            return block.ReThrow();       \
    }

#define V8TRYCATCH_VOID(type, var, value) \
    type var;                             \
    {                                     \
        v8::TryCatch block;               \
        var = (value);                    \
        if (block.HasCaught()) {          \
            block.ReThrow();              \
            return;                       \
        }                                 \
    }

#define V8TRYCATCH_WITH_TYPECHECK_VOID(type, var, value, isolate) \
    type var;                                                     \
    {                                                             \
        bool ok = true;                                           \
        {                                                         \
            v8::TryCatch block;                                   \
            var = (value);                                        \
            if (block.HasCaught()) {                              \
                block.ReThrow();                                  \
                return;                                           \
            }                                                     \
        }                                                         \
        if (UNLIKELY(!ok)) {                                      \
            throwTypeError(isolate);                              \
            return;                                               \
        }                                                         \
    }

#define V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(type, var, value, retVal) \
    type var(value);                                                     \
    if (!var.prepare())                                                  \
        return retVal;

#define V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(type, var, value) \
    type var(value);                                                 \
    if (!var.prepare())                                              \
        return;

} // namespace WebCore

#endif // V8BindingMacros_h
