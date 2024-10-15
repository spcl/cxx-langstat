#ifndef VARIABLETEMPLATEANALYSIS_H
#define VARIABLETEMPLATEANALYSIS_H

#include <string_view>

#include "cxx-langstat/Analysis.h"

//-----------------------------------------------------------------------------
// Enum to indicate what kind (idiom or language construct) is used to indicate
// a variable family (a family is what a template defines).
enum FamilyKind {
    ClassTemplateStaticMemberVar, ConstexprFunctionTemplate, VarTemplate
};
struct VariableFamily {
    VariableFamily() = default;
    VariableFamily(int Location, FamilyKind Kind) : Location(Location),
        Kind(Kind){}
    int Location;
    FamilyKind Kind;
};

//-----------------------------------------------------------------------------

class VariableTemplateAnalysis : public Analysis {
public:
    VariableTemplateAnalysis(){
        std::cout << "VTA ctor\n";
    }
    ~VariableTemplateAnalysis(){
        std::cout << "VTA dtor\n";
    }
    std::string_view getShorthand() override {
        return ShorthandName;
    }
protected:
    void extractFeatures();
    std::vector<VariableFamily> VariableFamilies;
    void analyzeFeatures() override;
    void processFeatures(const nlohmann::ordered_json& j) override;
    //
    static constexpr auto ShorthandName = "vta";
};

//-----------------------------------------------------------------------------

#endif // VARIABLETEMPLATEANALYSIS_H
