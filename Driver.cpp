// Sources for the ASTFrontendAction-ASTConsumer-MatchCallback "triad":
// Also has info on how to use FrontendActionFactory and ClangTool
// https://github.com/peter-can-talk/cppnow-2017/blob/master/code/mccabe/mccabe.cpp
// and the corresponding conference video
// https://www.youtube.com/watch?v=E6i8jmiy8MY

// Information about ASTFrontendAction, ASTConsumer from official Clang doc.
// https://clang.llvm.org/docs/RAVFrontendAction.html

// Information about ASTFrontendAction, ASTContext and Clang AST in general:
// https://jonasdevlieghere.com/understanding-the-clang-ast/


// clang includes
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Serialization/ASTWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "llvm/TargetParser/Host.h"
// standard includes
#include <iostream> // should be removed
#include <fstream> // file stream
#include <cstdlib> // setenv
#include <string>
// JSON library
#include <nlohmann/json.hpp>
//
#include "cxx-langstat/AnalysisRegistry.h"
#include "cxx-langstat/Utils.h"
#include "cxx-langstat/Driver.h"
#include "cxx-langstat/Stats.h"
#include "cxx-langstat/FeaturesPostProcessing.h"


using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using StringRef = llvm::StringRef;
using ASTContext = clang::ASTContext;
using ordered_json = nlohmann::ordered_json;

//-----------------------------------------------------------------------------
// Consumes the AST, i.e. does computations on it
class Consumer : public ASTConsumer {
public:
    Consumer(StringRef InFile, AnalysisRegistry* Registry) :
        InFile(InFile),
        Registry(Registry){
    }
    // Called when AST for TU is ready/has been parsed
    // Assumes -emit-features in on
    void HandleTranslationUnit(ASTContext& Context){
        ordered_json AllAnalysesFeatures;
        Registry->createFreshAnalyses();
        for(const auto& an : Registry->Analyses){
            // std::cout << "Starting analysis " << an->getShorthand() << std::endl;
            auto AnalysisShorthand = an->getShorthand();
            // Analyze clang AST and get features
            AllAnalysesFeatures[AnalysisShorthand] = an->getFeatures(InFile, Context);
            // std::cout << "Finished analysis " << an->getShorthand() << std::endl;
        }
        // Write to file if -emit-features is active
        auto OutputFile = Registry->getCurrentOutputFile();
        std::cout << "Writing features to file: " << OutputFile << std::endl;
        std::ofstream o(OutputFile);
        o << AllAnalysesFeatures.dump(4) << std::endl;
        Registry->destroyAnalyses();
    }
private:
    StringRef InFile;
    AnalysisRegistry* Registry;
};

// Responsible for steering when what is executed
class Action : public ASTFrontendAction {
public:
    Action(AnalysisRegistry* Registry) : Registry(Registry) {
        // std::cout << "Creating AST Action" << std::endl;
    }
    bool PrepareToExecuteAction(CompilerInstance& CI) override {
        return true;
    }

    // Called at start of processing a single input
    bool BeginSourceFileAction(CompilerInstance& Compiler) override {
        std::cout
        << "Starting to process \033[32m" << getCurrentFile().str()
        << "\033[0m. AST=" << isCurrentFileAST() << std::endl;
        
        return true;
    }
    // Called after frontend is initialized, but before per-file processing
    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
        CompilerInstance& CI, StringRef InFile) override {
            return std::make_unique<Consumer>(getCurrentFile(), Registry);
    }
    //
    void EndSourceFileAction() override {
        // std::cout << "Finished processing " << getCurrentFile().str() << ".\n";
    }
private:
    AnalysisRegistry* Registry;
};

// Responsible for building Actions
class Factory : public clang::tooling::FrontendActionFactory {
public:
    Factory(AnalysisRegistry* Reg) : Registry(Reg) {}
    //
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<Action>(Registry);
    }
private:
    AnalysisRegistry* Registry;
};

//-----------------------------------------------------------------------------
//
int CXXLangstatMain(std::vector<std::string> InputFiles,
    std::vector<std::string> OutputFiles, Stage Stage, std::string Analyses,
    std::string BuildPath, std::shared_ptr<CompilationDatabase> db){

    // Create custom options object for registry
    CXXLangstatOptions Opts(Stage, OutputFiles, Analyses);
    AnalysisRegistry* Registry = new AnalysisRegistry(Opts);

    int return_code = 0;

    // setenv("LIBCLANG_DISABLE_PCH_VALIDATION", "1", 1);
    if(Stage == emit_features){
        // https://clang.llvm.org/doxygen/CommonOptionsParser_8cpp_source.html @ 109
        // Read in database found in dir specified by -p or a parent path
        std::string ErrorMessage;
        if(!BuildPath.empty()){
            std::cout << "READING BUILD PATH\n";
            std::cout << "BUILD PATH: " << BuildPath << std::endl;
            db = CompilationDatabase::autoDetectFromDirectory(BuildPath, ErrorMessage);
            if(!ErrorMessage.empty()){
                std::cout << ErrorMessage << std::endl;
                exit(1);
            }
            std::cout << "FOUND COMPILE COMMANDS:" << std::endl;
            for(auto cc : db->getAllCompileCommands()){
                for(auto s : cc.CommandLine)
                    std::cout << s << " ";
                std::cout << std::endl;
            }
        }
        if(db){
            // Iterate over the input files
            for (const auto &FilePath : InputFiles) {
                if (llvm::sys::path::extension(FilePath) == ".ast") {

                    clang::CompilerInstance CI;
                    CI.createDiagnostics();

                    std::shared_ptr<clang::TargetOptions> TO = std::make_shared<clang::TargetOptions>();
                    // TO->Triple = "x86_64-pc-win32"; // see clang -v
                    TO->Triple = llvm::sys::getDefaultTargetTriple();
                    CI.setTarget(clang::TargetInfo::CreateTargetInfo(CI.getDiagnostics(), TO));

                    std::shared_ptr<ASTUnit> AST = ASTUnit::LoadFromASTFile(
                        FilePath, //const std::string &Filename, 
                        CI.getPCHContainerReader(), //const PCHContainerReader &PCHContainerRdr,
                        ASTUnit::LoadASTOnly, //WhatToLoad ToLoad, 
                        &CI.getDiagnostics(), //IntrusiveRefCntPtr<DiagnosticsEngine> Diags,
                        CI.getFileSystemOpts(), //const FileSystemOptions &FileSystemOpts,
                        std::make_shared<HeaderSearchOptions>(CI.getHeaderSearchOpts()), //std::shared_ptr<HeaderSearchOptions> HSOpts, 
                        false, //bool OnlyLocalDecls,
                        CaptureDiagsKind::All, //CaptureDiagsKind CaptureDiagnostics, 
                        false, //bool AllowASTWithCompilerErrors,
                        false, //bool UserFilesAreVolatile, 
                        llvm::vfs::getRealFileSystem() //IntrusiveRefCntPtr<llvm::vfs::FileSystem> VFS
                    );

                    if (!AST) {
                        std::cout << "Failed to load AST file: " << FilePath << ". Probably due to a missing header." << std::endl;
                        // return 1;

                        // continue analyzing other ASTs in this input list
                        continue;
                    }

                    // Create a CompilerInstance
                    // CompilerInstance CI;
                    clang::CompilerInvocation compilerInvocation;
                    CI.setInvocation(std::make_shared<CompilerInvocation>(compilerInvocation));
                    CI.setFileManager(&AST->getFileManager());
                    CI.setSourceManager(&AST->getSourceManager());
                    CI.setASTContext(&AST->getASTContext());

                    // Run the ASTFrontendAction
                    Action LangstatAction(Registry);

                    FrontendInputFile FIF(FilePath, InputKind(Language::Unknown, InputKind::Precompiled));
                    LangstatAction.BeginSourceFile(CI, FIF);
                    
                    llvm::Error err = LangstatAction.Execute();
                    if (err) {
                        std::cout << "Error processing AST file: " << FilePath << "\n";
                        // return 1;
                        
                        // continue analyzing other ASTs in this input list
                        continue;
                    }

                    LangstatAction.EndSourceFile();
                } else {
                    std::cout << "Not implemented non-ast files yet" << std::endl;
                    continue;
                }
            }
        } else {
            std::cout << "The tool couldn't run due to missing compilation database "
                "Please try one of the following:\n"
                " When analyzing .ast files, append \"--\", to indicate no CDB is necessary\n"
                " When analyzing .cpp files, append \"-- <compile flags>\" or \"-p <build path>\"\n";
            return_code = -1; // completely arbitrary
        }
    }

    // Process features stored on disk to statistics
    else if(Stage == emit_statistics){
        ordered_json AllFilesAllStatistics;
        ordered_json Summary;
        for(auto File : InputFiles){
            std::cout << "Reading features from file: " << File << "...";
            ordered_json j;
            std::ifstream i(File);
            i >> j;
            std::cout << "Done\n";
            ordered_json OneFileAllStatistics_individual;
            ordered_json OneFileAllStatistics_overall;
            Registry->createFreshAnalyses();
            for(const auto& an : Registry->Analyses){ // ref to unique_ptr bad?
                std::cout << "Running emit-statistics for " << an->getShorthand() << std::endl;
                auto AnalysisShorthand = an->getShorthand();

                ordered_json postProcessedFeatures = FeaturesPostProcessing::postProcessFeaturesWrapper(AnalysisShorthand, j[AnalysisShorthand]);
                std::cout << "Finished post processing for analysis " << an->getShorthand() << std::endl;

                for(const auto& [statdesc, stats] : an->getStatistics(j[AnalysisShorthand]).items()){
                    OneFileAllStatistics_individual[statdesc] = stats;
                }

                for(const auto& [statdesc, stats] : an->getStatistics(postProcessedFeatures).items()){
                    OneFileAllStatistics_overall[statdesc] = stats;
                }
            }

            Summary = add(std::move(Summary), OneFileAllStatistics_overall);
            AllFilesAllStatistics[File] = OneFileAllStatistics_individual;
            Registry->destroyAnalyses();
        }
        std::ofstream o(Registry->Options.OutputFiles[0]);
        o << AllFilesAllStatistics.dump(4) << std::endl;
        o << Summary.dump(4) << std::endl;

        return_code = 0;
    }

    delete Registry;
    return return_code;
}
