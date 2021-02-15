#ifndef ALGORITHMLIBRARYANALYSIS_H
#define ALGORITHMLIBRARYANALYSIS_H

#include "cxx-langstat/Analysis.h"
#include "cxx-langstat/Analyses/TemplateInstantiationAnalysis.h"

//-----------------------------------------------------------------------------
// A standard library analysis is a template instantiation analysis.
class AlgorithmLibraryAnalysis : public TemplateInstantiationAnalysis {
public:
    AlgorithmLibraryAnalysis();
    ~AlgorithmLibraryAnalysis(){
        std::cout << "ALA dtor\n";
    }
private:
    void processFeatures(nlohmann::ordered_json j) override;
    static std::unique_ptr<Analysis> Create(){
        
        return std::make_unique<AlgorithmLibraryAnalysis>();
    }
    static bool s_registered;
};

//-----------------------------------------------------------------------------

#endif // ALGORITHMLIBRARYANALYSIS_H
