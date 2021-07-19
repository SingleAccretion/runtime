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
            m_compiler->compCurBB = block;

            for (Statement* stmt : block->Statements())
            {
                m_compiler->compCurStmt = stmt;

                WalkTree(stmt->GetRootNodePointer(), nullptr);

                if (m_remorphingNeeded)
                {
                    JITDUMP("\nRemorhing and relinking " FMT_STMT "\n\n", stmt->GetID());

                    stmt->SetRootNode(m_compiler->fgMorphTree(stmt->GetRootNode()));
                    m_compiler->fgSetStmtSeq(stmt);

                    JITDUMP("Early lowering modified statement:\n");
                    DISPSTMT(stmt);
                    JITDUMP("\n");

                    phaseStatus        = PhaseStatus::MODIFIED_EVERYTHING;
                    m_remorphingNeeded = false;
                }
            }
        }

        m_compiler->compCurBB   = nullptr;
        m_compiler->compCurStmt = nullptr;

        return phaseStatus;
    }

    fgWalkResult PreOrderVisit(GenTree** use, GenTree* user)
    {
        GenTree* tree = *use;

        CorInfoHelpFunc helper = CORINFO_HELP_UNDEF;
        switch (tree->OperGet())
        {
#if !defined(TARGET_64BIT)
            case GT_MUL:
                if (tree->TypeIs(TYP_LONG) && !m_compiler->fgRecognizeLongMul(tree->AsOp()))
                {
                    helper = CORINFO_HELP_LMUL;
                    if (tree->gtOverflow())
                    {
                        helper = tree->IsUnsigned() ? CORINFO_HELP_ULMUL_OVF : CORINFO_HELP_LMUL_OVF;
                    }
                }
                break;

            case GT_DIV:
            case GT_UDIV:
#if USE_HELPERS_FOR_INT_DIV
                if (tree->TypeIs(TYP_INT))
                {
                    helper = tree->OperIs(GT_DIV) ? CORINFO_HELP_DIV : CORINFO_HELP_UDIV;
                }
#endif
                if (tree->TypeIs(TYP_LONG))
                {
                    helper = tree->OperIs(GT_DIV) ? CORINFO_HELP_LDIV : CORINFO_HELP_ULDIV;
                }
                break;
#endif // !defined(TARGET_64BIT)

            default:
                break;
        }

        if (helper != CORINFO_HELP_UNDEF)
        {
            *use = LowerIntoHelper(tree->AsOp(), helper);
        }

        return fgWalkResult::WALK_CONTINUE;
    }

private:
    GenTree* LowerIntoHelper(GenTreeOp* tree, CorInfoHelpFunc helper)
    {
        GenTreeCall::Use* args = m_compiler->gtNewCallArgs(tree->gtGetOp1(), tree->gtGetOp2());
        GenTreeCall*      call = m_compiler->gtNewHelperCallNode(helper, TYP_LONG, args);
        m_remorphingNeeded     = true;

        JITDUMP("Replacing GT_%s:\n", GenTree::OpName(tree->OperGet()));
        DISPNODE(tree);
        JITDUMP("With a call to helper:\n");
        DISPNODE(call);

        return call;
    }
};

PhaseStatus Compiler::fgEarlyLowering()
{
    EarlyLower earlyLower(this);

    return earlyLower.RunPhase();
}
