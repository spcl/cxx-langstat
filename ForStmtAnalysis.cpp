#include "clang/ASTMatchers/ASTMatchers.h"
#include <iostream>
#include "ForStmtAnalysis.h"

using namespace clang; // Stmt, Expr, FullSourceLoc
using namespace clang::ast_matchers;

// TODO: variables defined here shouldn't be visible in Langstat.cpp, only
// forstmt object should be

namespace fsa {

// Constructs matcher that exactly matches for-loops with depth d (nesting depth)
StatementMatcher constructMatcher(int d, std::string Name){
    StatementMatcher AtLeastDepth = anything();
    int i=1;
    while(i<d){
        AtLeastDepth = hasDescendant(forStmt(AtLeastDepth));
        i++;
    }
    StatementMatcher EnsureIsOuterMostLoop = unless(hasAncestor(forStmt()));
    StatementMatcher AtLeastDepthPlus1 = forStmt(EnsureIsOuterMostLoop, hasDescendant(forStmt(AtLeastDepth)));
    AtLeastDepth = forStmt(EnsureIsOuterMostLoop, AtLeastDepth);
    return forStmt(AtLeastDepth, unless(AtLeastDepthPlus1)).bind(Name);
}

// matchers with more depth could either be hardcoded
// or made constexpr if max-depth is fixed to speed up runtime
int depth = 1;


std::string Name = "fs";
// StatementMatcher BaseMatcher = forStmt();
// StatementMatcher test = forStmt(hasDescendant(BaseMatcher));
StatementMatcher m = constructMatcher(depth, Name);

auto Runner = [](const clang::ast_matchers::MatchFinder::MatchResult& Result) {
    const ForStmt* FS = Result.Nodes.getNodeAs<ForStmt>(Name);
    clang::ASTContext* Context = Result.Context;
    /*const Stmt* LoopInit = FS->getInit();
    const Expr* LoopCond = FS->getCond();
    const Expr* LoopInc = FS->getInc();*/
    if(FS){
        FullSourceLoc Location = Context->getFullLoc(FS->getBeginLoc());
        if(Location.isValid()){
            int LineNumber = Location.getLineNumber();
            std::cout << "for loop of depth " << depth << " @ line " << LineNumber << "\n";
            /*LoopInit->dumpPretty(*Context);
            LoopCond->dumpPretty(*Context);
            LoopInc->dumpPretty(*Context);*/
        }
    }
};

ForStmtAnalysis forstmt(Name, m, Runner);

} // namespace fsa
