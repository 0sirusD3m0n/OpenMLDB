/*-------------------------------------------------------------------------
 * Copyright (C) 2020, 4paradigm
 * physical_pass.h
 *
 * Author: chenjing
 * Date: 2020/3/13
 *--------------------------------------------------------------------------
 **/
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "passes/physical/physical_pass.h"
#include "vm/physical_op.h"

#ifndef SRC_PASSES_PHYSICAL_BATCH_REQUEST_OPTIMIZE_H_
#define SRC_PASSES_PHYSICAL_BATCH_REQUEST_OPTIMIZE_H_

namespace fesql {
namespace vm {

using fesql::base::Status;

/**
 * Split op with common columns to common and non-common parts.
 */
class CommonColumnOptimize : public PhysicalPass {
 public:
    explicit CommonColumnOptimize(const std::set<size_t> common_column_indices);

    Status Apply(PhysicalPlanContext* ctx, PhysicalOpNode* input,
                 PhysicalOpNode** out) override;

    void ExtractCommonNodeSet(std::set<size_t>* output);

 private:
    void Init();

    // mapping from origin op -> new ops
    struct BuildOpState {
        PhysicalOpNode* common_op = nullptr;
        PhysicalOpNode* non_common_op = nullptr;
        PhysicalOpNode* concat_op = nullptr;
        PhysicalOpNode* reordered_op = nullptr;
        std::set<size_t> common_column_indices;

        void AddCommonIdx(size_t idx) { common_column_indices.insert(idx); }

        void SetAllCommon(PhysicalOpNode* op) {
            common_op = op;
            non_common_op = nullptr;
            for (size_t i = 0; i < op->GetOutputSchemaSize(); ++i) {
                common_column_indices.insert(i);
            }
        }

        void SetAllNonCommon(PhysicalOpNode* op) {
            common_op = nullptr;
            non_common_op = op;
            common_column_indices.clear();
        }

        bool IsInitialized() const {
            return common_op != nullptr || non_common_op != nullptr;
        }
    };

    /**
     * For each original op, split into op product common part and op
     * produce non-common part. Null on one side indicates there is no
     * common part or no non-common part outputs.
     */
    Status GetOpState(PhysicalPlanContext* ctx, PhysicalOpNode* input,
                      BuildOpState** state);

    /**
     * Get concat op of common and non-common part.
     */
    Status GetConcatOp(PhysicalPlanContext* ctx, PhysicalOpNode* input,
                       PhysicalOpNode** out);

    /**
     * Get concat op of common and non-common part with original column order.
     * The output op take the same schema slice count with original op.
     */
    Status GetReorderedOp(PhysicalPlanContext* ctx, PhysicalOpNode* input,
                          PhysicalOpNode** out);

    Status ProcessRequest(PhysicalPlanContext*, PhysicalRequestProviderNode*,
                          BuildOpState*);
    Status ProcessData(PhysicalPlanContext*, PhysicalDataProviderNode*,
                       BuildOpState*);
    Status ProcessSimpleProject(PhysicalPlanContext*,
                                PhysicalSimpleProjectNode*, BuildOpState*);
    Status ProcessProject(PhysicalPlanContext*, PhysicalProjectNode*,
                          BuildOpState*);
    Status ProcessWindow(PhysicalPlanContext*, PhysicalAggrerationNode*,
                         BuildOpState*);
    Status ProcessJoin(PhysicalPlanContext*, PhysicalRequestJoinNode*,
                       BuildOpState*);
    Status ProcessConcat(PhysicalPlanContext*, PhysicalRequestJoinNode*,
                         BuildOpState*);
    Status ProcessRename(PhysicalPlanContext*, PhysicalRenameNode*,
                         BuildOpState*);
    Status ProcessTrivial(PhysicalPlanContext* ctx, PhysicalOpNode* op,
                          BuildOpState*);
    Status ProcessRequestUnion(PhysicalPlanContext*, PhysicalRequestUnionNode*,
                               const std::vector<PhysicalOpNode*>& path,
                               PhysicalOpNode** out);

    /**
     * Find a non-agg op sequence ends with request union.
     */
    bool FindRequestUnionPath(PhysicalOpNode*, std::vector<PhysicalOpNode*>*);

    void SetAllCommon(PhysicalOpNode*);

    // input common column indices
    std::set<size_t> common_column_indices_;

    std::unordered_map<size_t, BuildOpState> build_dict_;
};

}  // namespace vm
}  // namespace fesql
#endif  // SRC_PASSES_PHYSICAL_BATCH_REQUEST_OPTIMIZE_H_
