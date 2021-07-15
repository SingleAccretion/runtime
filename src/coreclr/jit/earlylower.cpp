// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

/*XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XX                                                                           XX
XX                               Early lowering                              XX
XX                                                                           XX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
*/

#include "jitpch.h"

class EarlyLower : public GenTreeVisitor<EarlyLower>
{
private:
    bool m_remorphingNeeded;

public:
    EarlyLower(Compiler* compiler) : GenTreeVisitor<EarlyLower>(compiler)
        , m_remorphingNeeded(false)
    {
    }

    enum
    {
        DoPreOrder = true
    };

    PhaseStatus RunPhase()
    {
        PhaseStatus phaseStatus = PhaseStatus::MODIFIED_NOTHING;

        for (BasicBlock* block : m_compiler->Blocks())
        {
            for (Statement* stmt : block->Statements())
            {
                WalkTree(stmt->GetRootNodePointer(), nullptr);

                if (m_remorphingNeeded)
                {
                    JITDUMP("\nRemorhing and relinking " FMT_STMT "\n\n", stmt->GetID());

                    m_compiler->fgMorphTree(stmt->GetRootNode());
                    m_compiler->fgSetStmtSeq(stmt);

                    JITDUMP("Early lowering modified statement:\n");
                    DISPSTMT(stmt);

                    phaseStatus        = PhaseStatus::MODIFIED_EVERYTHING;
                    m_remorphingNeeded = false;
                }
            }
        }

        return phaseStatus;
    }

    fgWalkResult PreOrderVisit(GenTree** use, GenTree* user)
    {
        GenTree* tree = *use;

        switch (tree->OperGet())
        {
#if !defined(TARGET_64BIT)
            case GT_MUL:
                if (tree->TypeIs(TYP_LONG) && !m_compiler->fgRecognizeLongMul(tree->AsOp()))
                {
                    *use = LowerMulIntoHelpers(tree->AsOp());
                }
                break;
#endif // !defined(TARGET_64BIT)

            default:
                break;
        }

        return fgWalkResult::WALK_CONTINUE;
    }

private:
#if !defined(TARGET_64BIT)
    GenTree* LowerMulIntoHelpers(GenTreeOp* mul)
    {
        CorInfoHelpFunc helper = CORINFO_HELP_LMUL;
        if (mul->gtOverflow())
        {
            helper = mul->IsUnsigned() ? CORINFO_HELP_ULMUL_OVF : CORINFO_HELP_LMUL_OVF;
        }

        GenTreeCall::Use* args = m_compiler->gtNewCallArgs(mul->gtGetOp1(), mul->gtGetOp2());
        GenTreeCall*      call = m_compiler->gtNewHelperCallNode(helper, TYP_LONG, args);
        m_remorphingNeeded     = true;

        JITDUMP("Replacing GT_MUL:\n");
        DISPNODE(mul);
        JITDUMP("With a call to helper:\n");
        DISPNODE(call);

        return call;
    }
#endif // !defined(TARGET_64BIT)
};

PhaseStatus Compiler::fgEarlyLowering()
{
    EarlyLower earlyLower(this);

    return earlyLower.RunPhase();
}
