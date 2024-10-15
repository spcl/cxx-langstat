#include <iostream>
#include <vector>

#include "cxx-langstat/Analysis.h"
#include "cxx-langstat/Analyses/LoopDepthAnalysis.h"
#include "cxx-langstat/Utils.h"

using namespace clang::ast_matchers;
using ordered_json = nlohmann::ordered_json;

//-----------------------------------------------------------------------------
// Constructs matcher that exactly matches mixed loops with depth d (nesting depth)
StatementMatcher constructMixedMatcher(std::string Name, int d){
    auto loopStmt = [](auto m){
        return stmt(
            // isExpansionInMainFile(),
            isExpansionInHomeDirectory(),
            unless(isExpansionInSystemHeader()),
            anyOf(forStmt(m), whileStmt(m), doStmt(m), cxxForRangeStmt(m)));
    };
    StatementMatcher NumOfDescendantsAtLeastD = anything();
    int i=1;
    while(i<d){
        NumOfDescendantsAtLeastD = hasDescendant(
            loopStmt(NumOfDescendantsAtLeastD));
        i++;
    }
    StatementMatcher NumOfDescendantsAtLeastDPlus1 =
        hasDescendant(loopStmt(NumOfDescendantsAtLeastD));
    StatementMatcher NumOfDescendantsExactlyD =
        allOf(NumOfDescendantsAtLeastD, unless(NumOfDescendantsAtLeastDPlus1));
    StatementMatcher IsOuterMostLoop =
        unless(hasAncestor(loopStmt(anything())));
    StatementMatcher LoopHasExactlyDepthD =
        allOf(IsOuterMostLoop, NumOfDescendantsExactlyD);
    return loopStmt(LoopHasExactlyDepthD).bind(Name);
}

//-----------------------------------------------------------------------------

void LoopDepthAnalysis::extractFeatures() {
    // Bind is necessary to retrieve information about the match like location etc.
    // Without bind the match is still registered, thus we can still count #matches,
    // but nothing else

    // Depth analysis
    StatementMatcher IsOuterMostLoop =
        unless(hasAncestor(stmt(anyOf(
            forStmt(), whileStmt(), doStmt(), cxxForRangeStmt()))));
    TopLevelLoops = this->Extractor.extract(*Context, "fs1", stmt(
        // isExpansionInMainFile(),
        isExpansionInHomeDirectory(),
        unless(isExpansionInSystemHeader()),
        anyOf(
            forStmt(IsOuterMostLoop),
            whileStmt(IsOuterMostLoop),
            doStmt(IsOuterMostLoop),
            cxxForRangeStmt(IsOuterMostLoop)))
    .bind("fs1"));

    unsigned NumLoopsFound = 0;
    for (int i=1; i<=this->MaxDepth; i++){
        StatementMatcher Matcher = constructMixedMatcher("fs", i);
        auto matches = this->Extractor.extract(*Context, "fs", Matcher);
        NumLoopsFound += matches.size();
        LoopsOfDepth.emplace_back(matches);
        // stop searching if all possible loops have been found
        if(NumLoopsFound == TopLevelLoops.size())
            break;
    }
    if(NumLoopsFound < TopLevelLoops.size())
        std::cout << "Some loops deeper than max_depth have not been"
                     " analyzed\n";
}

void LoopDepthAnalysis::analyzeFeatures(){
    extractFeatures();
    ordered_json loops;
    unsigned depth = 1;
    for (auto matches : LoopsOfDepth){
        unsigned d = matches.size();
        if (d!=0){
            std::vector<unsigned> locations;
            for (auto m : matches){
                locations.emplace_back(m.Location);
            }
            loops[std::to_string(depth)] = locations;
        }
        depth++;
    }
    Features = loops;
}

void LoopDepthAnalysis::processFeatures(const nlohmann::ordered_json& j){
    std::map<std::string, unsigned> m;
    for(const auto& [depth, locations] : j.items()){
        m.try_emplace(depth, locations.size());
    }
    std::string desc = "distribution of loop depths";
    Statistics[desc] = m;
}

//-----------------------------------------------------------------------------
