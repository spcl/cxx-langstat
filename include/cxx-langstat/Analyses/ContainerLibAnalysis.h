#ifndef CONTAINERLIBANALYSIS_H
#define CONTAINERLIBANALYSIS_H

#include "cxx-langstat/Analysis.h"
#include "cxx-langstat/Analyses/TemplateInstantiationAnalysis.h"

//-----------------------------------------------------------------------------

class ContainerLibAnalysis : public TemplateInstantiationAnalysis {
public:
    ContainerLibAnalysis();
    ~ContainerLibAnalysis(){
        std::cout << "CLA dtor\n";
    }
    std::string getShorthand() override {
        return ShorthandName;
    }
private:
    void processFeatures(nlohmann::ordered_json j) override;
    static std::unique_ptr<Analysis> Create(){
        return std::make_unique<ContainerLibAnalysis>();
    }
    static bool s_registered;
    static constexpr auto ShorthandName = "cla";
};

//-----------------------------------------------------------------------------

#endif // CONTAINERLIBANALYSIS_H
