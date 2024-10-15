#include <iostream>
#include <vector>

#include "cxx-langstat/Analysis.h"
#include "cxx-langstat/Analyses/VariableTemplateAnalysis.h"
#include "cxx-langstat/Utils.h"

using namespace clang;
using namespace clang::ast_matchers;
using ordered_json = nlohmann::ordered_json;

//-----------------------------------------------------------------------------
// Question: did programmers abandon classes with static fields and constexpr
// functions returning the value in favor of variable templates, which were
// introduced in C++14?

void VariableTemplateAnalysis::extractFeatures(){
    // First pre-C++14 idiom to get variable template functionality:
    // Class templates with static data.
    // static members of classes in AST are varDecls, not fieldDecls.
    // Essentially the same matcher as for typedefs in UsingAnalysis.cpp,
    // but static vardecl instead of typedefs.
    DeclarationMatcher ClassWithStaticMember = classTemplateDecl(
        // isExpansionInMainFile(),
        isExpansionInHomeDirectory(),
        unless(isExpansionInSystemHeader()),
        has(cxxRecordDecl(
            // Must have static member
            forEach(varDecl(isStaticStorageClass()).bind("staticmember")),
            // Mustn't have decl that is not static member, access spec or
            // implicit cxxrecord ()
            // Why does cxxRecordDecl inside class template contain implicit
            // implicit cxxRecordDecl?
            unless(has(decl(
                unless(anyOf(
                    varDecl(isStaticStorageClass()),
                    accessSpecDecl(),
                    cxxRecordDecl(isImplicit())))))))))
    .bind("classwithstaticmember");

    // Second pre-C++14 idiom:
    DeclarationMatcher ConstexprFunction = functionTemplateDecl(
        // isExpansionInMainFile(),
        isExpansionInHomeDirectory(),
        unless(isExpansionInSystemHeader()),
        has(functionDecl(
            isConstexpr(),
            // does not have "void" return type
            unless(returns(asString("void"))),
            // body
            has(compoundStmt(
                anyOf(
                    // at least a variable declaration and a return that returns it
                    allOf(
                        forEach(declStmt(has(varDecl().bind("datadecl")))),
                        has(returnStmt(has(
                            declRefExpr(to(varDecl(equalsBoundNode("datadecl")))))))),
                    // or a return stmt returning some expr
                    has(returnStmt(hasReturnValue(expr())))),
                // and does not contain anything else. Sufficient to filter for
                // statement since function body contains statements only. Decl
                // inside of body handled via a declStmt - which is a Stmt too.
                unless(has(stmt(
                    unless(anyOf(
                        declStmt(has(varDecl())),
                        returnStmt()))))))))))
    .bind("constexprfunctiontemplate");

    // C++14 variable templates
    // "Write" matcher for variable templates that didn't exist yet:
    // http://clang.llvm.org/docs/LibASTMatchers.html#writing-your-own-matchers
    // https://clang.llvm.org/doxygen/ASTMatchersInternal_8cpp_source.html
    internal::VariadicDynCastAllOfMatcher<Decl, VarTemplateDecl> varTemplateDecl;
    DeclarationMatcher VariableTemplate = varTemplateDecl(
        // isExpansionInMainFile())
        isExpansionInHomeDirectory(),
        unless(isExpansionInSystemHeader()))
    .bind("variabletemplate");

    auto ClassWithStaticMemberDecls = Extractor.extract(*Context,
        "classwithstaticmember", ClassWithStaticMember);
    auto ConstexprFunctionDecls = Extractor.extract(*Context,
        "constexprfunctiontemplate", ConstexprFunction);
    auto VariableTemplateDecls = Extractor.extract(*Context,
        "variabletemplate", VariableTemplate);

    // Possible improvement: report identifier of class/function/variable
    // template.
    for(const auto& m : ClassWithStaticMemberDecls){
        VariableFamilies.emplace_back(VariableFamily(m.Location,
            ClassTemplateStaticMemberVar));
    }
    for(const auto& m : ConstexprFunctionDecls){
        VariableFamilies.emplace_back(VariableFamily(m.Location,
            ConstexprFunctionTemplate));
    }
    for(const auto& m : VariableTemplateDecls){
        VariableFamilies.emplace_back(VariableFamily(m.Location,
            VarTemplate));
    }
}

//-----------------------------------------------------------------------------
// Helper functions to convert SynonymKind to string and back.
std::string VFKToString(FamilyKind Kind){
    switch (Kind) {
        case ClassTemplateStaticMemberVar:
            return "static data member in class template";
        case ConstexprFunctionTemplate:
            return "Constexpr function template";
        case VarTemplate:
            return "Variable template";
        default:
            return "invalid";
    }
}
FamilyKind getVFKFromString(llvm::StringRef s){
    if(s.equals("static data member in class template"))
        return ClassTemplateStaticMemberVar;
    else if(s.equals("Constexpr function template"))
        return ConstexprFunctionTemplate;
    else
        return VarTemplate;
}
// Functions to convert structs to/from JSON.
void to_json(nlohmann::json& j, const VariableFamily& vf){
    j = nlohmann::json{{"location", vf.Location},{"kind", VFKToString(vf.Kind)}};
}
void from_json(const nlohmann::json& j, VariableFamily& vf){
    j.at("location").get_to(vf.Location);
    vf.Kind = getVFKFromString(j.at("kind").get<std::string>());
}
//-----------------------------------------------------------------------------
//
void VariableTemplateAnalysis::analyzeFeatures(){
    extractFeatures();
    for(auto vf : VariableFamilies){
        nlohmann::json vf_j = vf;
        Features.emplace_back(vf_j);
    }
}

void VariableFamilyKindPrevalence(ordered_json& Stats, const ordered_json& j){
    unsigned CTSDs = 0, CFTs = 0, VTs = 0;
    for(const auto& vf_j : j){
        // std::cout << vf_j.dump(4) << std::endl;
        VariableFamily vf;
        from_json(vf_j, vf);
        switch(vf.Kind){
            case ClassTemplateStaticMemberVar:
                CTSDs++;
                break;
            case ConstexprFunctionTemplate:
                CFTs++;
                break;
            case VarTemplate:
                VTs++;
                break;
        }
    }
    auto desc = "prevalence of variable templates and pre-C++14 idioms";
    Stats[desc]["static data member in class template"] = CTSDs;
    Stats[desc]["constexpr function template"] = CFTs;
    Stats[desc]["variable template"] = VTs;
}

void VariableTemplateAnalysis::processFeatures(const nlohmann::ordered_json& j){
    VariableFamilyKindPrevalence(Statistics, j);
}

//-----------------------------------------------------------------------------
