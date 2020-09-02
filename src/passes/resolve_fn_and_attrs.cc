/*-------------------------------------------------------------------------
 * Copyright (C) 2019, 4paradigm
 * resolve_fn_and_attrs.cc
 *--------------------------------------------------------------------------
 **/
#include "passes/resolve_fn_and_attrs.h"
#include <utility>
#include "passes/resolve_udf_def.h"

using ::fesql::common::kCodegenError;
using ::fesql::node::TypeEquals;

namespace fesql {
namespace passes {

Status ResolveFnAndAttrs::CheckSignature(
    node::FnDefNode* fn, const std::vector<const node::TypeNode*>& arg_types) {
    CHECK_TRUE(fn->GetArgSize() == arg_types.size(), kCodegenError,
               "Infer fn def failed, expect ", fn->GetArgSize(),
               " arguments but get ", arg_types.size());

    for (size_t i = 0; i < fn->GetArgSize(); ++i) {
        CHECK_TRUE(arg_types[i] != nullptr, kCodegenError, i,
                   "th actual argument type must be set non-null");
        auto expect_type = fn->GetArgType(i);
        if (expect_type == nullptr) {
            continue;  // skip unknown type, it should be resolved in this pass
        }
        CHECK_TRUE(TypeEquals(expect_type, arg_types[i]), kCodegenError,
                   "Infer fn def failed, expect ", expect_type->GetName(),
                   " at ", i, "th argument, but get ", arg_types[i]->GetName());
    }
    return Status::OK();
}

Status ResolveFnAndAttrs::VisitFnDef(
    node::FnDefNode* fn, const std::vector<const node::TypeNode*>& arg_types,
    node::FnDefNode** output) {
    switch (fn->GetType()) {
        case node::kLambdaDef: {
            node::LambdaNode* lambda = nullptr;
            CHECK_STATUS(VisitLambda(dynamic_cast<node::LambdaNode*>(fn),
                                     arg_types, &lambda));
            *output = lambda;
            break;
        }
        case node::kUDFDef: {
            node::UDFDefNode* udf_def = nullptr;
            CHECK_STATUS(VisitUDFDef(dynamic_cast<node::UDFDefNode*>(fn),
                                     arg_types, &udf_def));
            *output = udf_def;
            break;
        }
        case node::kUDAFDef: {
            node::UDAFDefNode* udaf_def = nullptr;
            CHECK_STATUS(VisitUDAFDef(dynamic_cast<node::UDAFDefNode*>(fn),
                                      arg_types, &udaf_def));
            *output = udaf_def;
            break;
        }
        default: {
            *output = fn;
        }
    }
    return Status::OK();
}

Status ResolveFnAndAttrs::VisitLambda(
    node::LambdaNode* lambda,
    const std::vector<const node::TypeNode*>& arg_types,
    node::LambdaNode** output) {
    // sanity checks
    CHECK_STATUS(CheckSignature(lambda, arg_types),
                 "Check lambda signature failed for\n",
                 lambda->GetTreeString());

    // bind lambda argument types
    for (size_t i = 0; i < arg_types.size(); ++i) {
        auto arg = lambda->GetArg(i);
        arg->SetOutputType(arg_types[i]);
        arg->SetNullable(true);
    }

    node::ExprNode* new_body = nullptr;
    CHECK_STATUS(VisitExpr(lambda->body(), &new_body),
                 "Resolve lambda body failed for\n", lambda->GetTreeString());
    lambda->SetBody(new_body);

    *output = lambda;
    return Status::OK();
}

Status ResolveFnAndAttrs::VisitUDFDef(
    node::UDFDefNode* udf_def,
    const std::vector<const node::TypeNode*>& arg_types,
    node::UDFDefNode** output) {
    // sanity checks
    CHECK_STATUS(CheckSignature(udf_def, arg_types),
                 "Check udf signature failed for\n", udf_def->GetTreeString());

    ResolveUdfDef udf_resolver;
    CHECK_STATUS(udf_resolver.Visit(udf_def->def()),
                 "Resolve udf definition failed for\n",
                 udf_def->GetTreeString());

    *output = udf_def;
    return Status::OK();
}

Status ResolveFnAndAttrs::VisitUDAFDef(
    node::UDAFDefNode* udaf,
    const std::vector<const node::TypeNode*>& arg_types,
    node::UDAFDefNode** output) {
    // sanity checks
    CHECK_STATUS(CheckSignature(udaf, arg_types),
                 "Check udaf signature failed for\n", udaf->GetTreeString());

    // visit init
    node::ExprNode* resolved_init = nullptr;
    if (udaf->init_expr() != nullptr) {
        CHECK_STATUS(VisitExpr(udaf->init_expr(), &resolved_init),
                     "Resolve init expr failed for ", udaf->GetName(), ":\n",
                     udaf->GetTreeString());
    }

    // get state type
    const node::TypeNode* state_type;
    if (resolved_init != nullptr) {
        state_type = resolved_init->GetOutputType();
    } else {
        state_type = udaf->GetElementType(0);
    }
    CHECK_TRUE(state_type != nullptr, kCodegenError,
               "Fail to resolve state type of udaf ", udaf->GetName());

    // visit update
    std::vector<const node::TypeNode*> update_arg_types;
    update_arg_types.push_back(state_type);
    for (auto list_type : arg_types) {
        CHECK_TRUE(list_type->base() == node::kList, kCodegenError);
        update_arg_types.push_back(list_type->GetGenericType(0));
    }
    node::FnDefNode* resolved_update = nullptr;
    CHECK_TRUE(udaf->update_func() != nullptr, kCodegenError);
    CHECK_STATUS(
        VisitFnDef(udaf->update_func(), update_arg_types, &resolved_update),
        "Resolve update function of ", udaf->GetName(), " failed");
    state_type = resolved_update->GetReturnType();
    CHECK_TRUE(state_type != nullptr, kCodegenError,
               "Fail to resolve state type of udaf ", udaf->GetName());

    // visit merge
    node::FnDefNode* resolved_merge = nullptr;
    if (udaf->merge_func() != nullptr) {
        CHECK_STATUS(VisitFnDef(udaf->merge_func(), {state_type, state_type},
                                &resolved_merge),
                     "Resolve merge function of ", udaf->GetName(), " failed");
    }

    // visit output
    node::FnDefNode* resolved_output = nullptr;
    if (udaf->output_func() != nullptr) {
        CHECK_STATUS(
            VisitFnDef(udaf->output_func(), {state_type}, &resolved_output),
            "Resolve output function of ", udaf->GetName(), " failed");
    }

    *output =
        nm_->MakeUDAFDefNode(udaf->GetName(), arg_types, resolved_init,
                             resolved_update, resolved_merge, resolved_output);
    CHECK_STATUS((*output)->Validate(arg_types), "Illegal resolved udaf: \n",
                 (*output)->GetTreeString());
    return Status::OK();
}

Status ResolveFnAndAttrs::VisitExpr(node::ExprNode* expr,
                                    node::ExprNode** output) {
    auto iter = cache_.find(expr);
    if (iter != cache_.end()) {
        *output = iter->second;
        return Status::OK();
    }
    for (size_t i = 0; i < expr->GetChildNum(); ++i) {
        node::ExprNode* old_child = expr->GetChild(i);
        node::ExprNode* new_child = nullptr;
        CHECK_STATUS(VisitExpr(old_child, &new_child), "Visit ", i,
                     "th child failed of\n", old_child->GetTreeString());
        if (new_child != nullptr && new_child != expr->GetChild(i)) {
            expr->SetChild(i, new_child);
        }
    }
    *output = expr;  // default
    switch (expr->GetExprType()) {
        case node::kExprCall: {
            auto call = dynamic_cast<node::CallExprNode*>(expr);
            auto external_fn =
                dynamic_cast<const node::ExternalFnDefNode*>(call->GetFnDef());
            if (external_fn == nullptr) {
                // non-external def
                node::FnDefNode* resolved_fn = nullptr;
                std::vector<const node::TypeNode*> arg_types;
                for (size_t i = 0; i < call->GetChildNum(); ++i) {
                    arg_types.push_back(call->GetChild(i)->GetOutputType());
                }
                CHECK_STATUS(
                    VisitFnDef(call->GetFnDef(), arg_types, &resolved_fn),
                    "Resolve function ", call->GetFnDef()->GetName(),
                    " failed");
                call->SetFnDef(resolved_fn);
                *output = call;

            } else {
                if (external_fn->IsResolved()) {
                    break;
                }
                node::ExprNode* result = nullptr;
                std::vector<node::ExprNode*> arg_list;
                for (size_t i = 0; i < call->GetChildNum(); ++i) {
                    arg_list.push_back(call->GetChild(i));
                }

                auto status = library_->Transform(external_fn->function_name(),
                                                  arg_list, nm_, &result);
                if (status.isOK() && result != nullptr) {
                    node::ExprNode* resolved_result = nullptr;
                    CHECK_STATUS(VisitExpr(result, &resolved_result));
                    *output = resolved_result;
                } else {
                    // fallback to legacy fn gen with warning
                    LOG(WARNING)
                        << "Resolve function '" << external_fn->function_name()
                        << "' failed, fallback to legacy: " << status;
                }
            }
            break;
        }
        default:
            break;
    }

    // Infer attr for non-group expr
    if ((*output)->GetExprType() != node::kExprList) {
        CHECK_STATUS((*output)->InferAttr(&analysis_context_), "Fail to infer ",
                     (*output)->GetExprString());
    }
    cache_.insert(iter, std::make_pair(expr, *output));
    return Status::OK();
}

}  // namespace passes
}  // namespace fesql
