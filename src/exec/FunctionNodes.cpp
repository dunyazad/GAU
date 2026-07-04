// Entry/Return/Call node generation for function definitions.

#include "FunctionNodes.h"

#include "core/Value.h"

#include <string>
#include <utility>
#include <vector>

namespace gau {

// Guards against runaway recursion and unbounded body loops.
static constexpr int MAX_CALL_DEPTH = 256;
static constexpr int MAX_BODY_STEPS = 100000;

void RegisterFunctionNodes(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                           TypeRegistry& types, const FunctionDef& def)
{
    const TypeId execId = types.Builtin(TypeTag::Exec);
    const bool hasExec = def.hasExec;
    const int inCount = static_cast<int>(def.inputs.size());
    const int outCount = static_cast<int>(def.outputs.size());
    const int off = hasExec ? 1 : 0;

    // Entry "<name> In": exec source (optional) plus one data output per
    // function input.
    NodeClass entry;
    entry.name = def.name + " In";
    entry.category = "Function";
    if (hasExec) {
        entry.pins.push_back(PinDef{PinDirection::Output, execId, "Then"});
    }
    for (const FunctionParam& p : def.inputs) {
        entry.pins.push_back(PinDef{PinDirection::Output, p.type, p.name});
    }
    classes.Register(entry);

    // Return "<name> Out": exec sink (optional) plus one data input per
    // function output.
    NodeClass ret;
    ret.name = def.name + " Out";
    ret.category = "Function";
    if (hasExec) {
        ret.pins.push_back(PinDef{PinDirection::Input, execId, "Exec"});
    }
    for (const FunctionParam& p : def.outputs) {
        ret.pins.push_back(PinDef{PinDirection::Input, p.type, p.name});
    }
    classes.Register(ret);

    // Call "<name>": exec in/out (optional), data inputs, data outputs.
    NodeClass call;
    call.name = def.name;
    call.category = "Function";
    if (hasExec) {
        call.pins.push_back(PinDef{PinDirection::Input, execId, "Exec"});
        call.pins.push_back(PinDef{PinDirection::Output, execId, "Then"});
    }
    for (const FunctionParam& p : def.inputs) {
        call.pins.push_back(PinDef{PinDirection::Input, p.type, p.name});
    }
    for (const FunctionParam& p : def.outputs) {
        call.pins.push_back(PinDef{PinDirection::Output, p.type, p.name});
    }
    classes.Register(call);

    // Entry behavior: hand each function input value to the body.
    builtins.Register(entry.name, [off, inCount](NodeEval& e) {
        for (int k = 0; k < inCount; ++k) {
            e.Out(off + k, e.rt->ParamIn(k));
        }
    });

    // Return behavior: collect the body's output values.
    builtins.Register(ret.name, [off, outCount](NodeEval& e) {
        for (int k = 0; k < outCount; ++k) {
            e.rt->SetResult(k, e.In(off + k));
        }
    });

    // Call behavior: run the body in a nested runtime with input marshalling.
    const NodeClassRegistry* classesPtr = &classes;
    const BuiltinRegistry* builtinsPtr = &builtins;
    TypeRegistry* typesPtr = &types;
    const FunctionDef* defPtr = &def;
    builtins.Register(call.name,
                      [classesPtr, builtinsPtr, typesPtr, defPtr, hasExec, off, inCount,
                       outCount](NodeEval& e) {
                          const int depth = e.rt->CallDepth() + 1;
                          if (depth > MAX_CALL_DEPTH) {
                              e.rt->Log("function call recursion limit exceeded: " + defPtr->name);
                              for (int k = 0; k < outCount; ++k) {
                                  e.Out(off + k, typesPtr->MakeDefault(defPtr->outputs[
                                      static_cast<std::size_t>(k)].type));
                              }
                              return;
                          }

                          std::vector<Value> params;
                          params.reserve(static_cast<std::size_t>(inCount));
                          for (int k = 0; k < inCount; ++k) {
                              params.push_back(e.In(off + k));
                          }

                          Runtime* parent = e.rt;
                          Runtime nested(*defPtr->body, *typesPtr, *classesPtr, *builtinsPtr,
                                         [parent](const std::string& m) { parent->Log(m); });
                          nested.SetCallDepth(depth);
                          nested.SetParamsIn(std::move(params));

                          if (hasExec) {
                              nested.Start(defPtr->entryNode);
                              nested.Run(MAX_BODY_STEPS);
                          } else {
                              nested.EvalNode(defPtr->returnNode);
                          }

                          for (int k = 0; k < outCount; ++k) {
                              e.Out(off + k, nested.ResultOut(k));
                          }
                      });
}

} // namespace gau
