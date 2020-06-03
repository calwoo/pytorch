#pragma once
#include <torch/csrc/jit/api/module.h>
#include <torch/csrc/jit/ir/ir.h>
#include <torch/csrc/jit/ir/subgraph_matcher.h>
#include <torch/csrc/jit/passes/graph_rewrite_helper.h>

#include <regex>

namespace torch {
namespace jit {

using graph_rewrite_helper::getFuncName;

// Vector of a module and the name of its method
using ModuleMethodVector = std::vector<std::pair<Module, std::string>>;
// Map of quantization parameter name and value
// for example _scale, _zero_point,
// _scalar_type and _axis(for per channel quantization)
using QParamVector = std::vector<std::pair<std::string, IValue>>;

// =========== helper functions for Value =========
// Check if a value is weight, since we need to use weight observer
// for weight
TORCH_API bool isWeight(Value* v);

// Check if a value is bias for conv and linear, which we do not
// quantize
TORCH_API bool isBiasOfConvOrLinear(Value* v);

// Check if the value may need observation or not
// one example for values that doesn't need observation is the
// scalar inputs for ops like add/mul
TORCH_API bool mayRequireObservation(Value* v);

// For a given value `v`, get the list of values that we need to check
// if they are observed/quantized or not, if so, we can say the
// `v` is also observed/quantized, since we can derive
// the quantization parameters for `v` given the list of values
TORCH_API std::vector<Value*> getPassThroughInputs(Value* v);

// =========== helper functions for Node =========
TORCH_API bool isSingleInputGeneralValueAtenFunction(Node* n);

TORCH_API bool isSingleInputGeneralCallFunction(Node* n);

TORCH_API bool isSingleInputGeneralAtenFunction(Node* n);

// Check if the node will produce the same result regardless of whether
// the input tensor is quantized or not, example: aten::size
TORCH_API bool isTensorInfoNode(Node* n);

// Check if this the the propaagate op that has single input, e.g. aten::cat
TORCH_API bool isPropagateQuantSingleInputOp(Node* n);

// Check if this is the propagate op that has two inputs, e.g. aten::add
TORCH_API bool isPropagateQuantBinaryOp(Node* n);

// Check if this is the node that we'll quantize or not quantize depending on
// whether the input of the node is quantized, example: aten::cat
TORCH_API bool isPropagateQuantNode(Node* n);

TORCH_API c10::optional<std::tuple<c10::QScheme, QParamVector>> getFixedQParams(
    Node* n);

// We don't want to analyze the graph for some `builtin` CallFunctions
// like `linear` because we want to preserve the op boundary
TORCH_API bool userDefinedCallFunction(Node* n);

// Check if the node has scalar input
TORCH_API bool hasScalarInput(Node* n);

// Check if a node is quantizable
TORCH_API bool nodeQuantizable(Node* n, bool is_dynamic = false);

// Check if a use of the value is quantizable, this depends on
// both the use node and the offset
TORCH_API bool useQuantizable(const Use& use, bool is_dynamic);

// Given a CallFunction node, extract the graph of the called function
TORCH_API std::shared_ptr<Graph> getCallFunctionGraph(Node* n);

// =========== helper functions for Block =========
// checks if a block will always raise an Exception
TORCH_API bool alwaysRaisesException(Block* block);

// =========== helper functions for Module  ==========
// TODO: remove
TORCH_API std::vector<std::string> getModuleAccessPath(
    Value* instance,
    Value* self);
// TODO: remove
TORCH_API Module
findChildModule(const Module& module, const std::vector<std::string>& path);

// Given an CallMethod node, get the module instance corresponding
// to the instance Value
TORCH_API Module getInvokedModule(Module& module, Node* n, Value* self);

// ==================== filter functions for matches ==============
auto aten_add_alpha_is_one = [](const Match& match,
                                const std::unordered_map<std::string, Value*>& vmap) {
    const auto& match_vmap = match.values_map;
    auto alpha = toIValue(match_vmap.at(vmap.at("alpha")));
    return alpha && alpha->isInt() && alpha->toInt() == 1;
};

auto is_functional_relu = [](const Match& match,
                             const std::unordered_map<std::string, Value*>& vmap) {
    const auto& match_vmap = match.values_map;
    Value* relu = match_vmap.at(vmap.at("relu"));
    return relu->type()->cast<FunctionType>() && getFuncName(relu) == "relu";
};

auto is_relu_module = [](const Match& match,
                         const std::unordered_map<std::string, Value*>& vmap) {
    const auto& match_vmap = match.values_map;
    Value* relu = match_vmap.at(vmap.at("relu"));
    auto type = relu->type()->cast<ClassType>();
    if (type && type->name()) {
      static std::regex mangle_re("\\.___torch_mangle_\\d+");
      auto qualified_name =
        std::regex_replace(type->name()->qualifiedName(), mangle_re, "");
      return qualified_name == "__torch__.torch.nn.modules.activation.ReLU";
    }
    return false;
};

} // namespace jit
} // namespace torch
