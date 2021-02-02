#include <iostream>
#include <future>

#include "llvm/Support/Commandline.h"
#include "clang/Tooling/CompilationDatabase.h"

#include "cxx-langstat/Options.h"
#include "../Driver.h"

#include <dirent.h>

using Twine = llvm::Twine;
using StringRef = llvm::StringRef;

using namespace clang::tooling;

//-----------------------------------------------------------------------------

// Global variables
// Options in CLI specific to cxx-langstat
llvm::cl::OptionCategory CXXLangstatCategory("cxx-langstat options", "");
llvm::cl::OptionCategory IOCategory("Input/output options", "");

// llvm::cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
// llvm::cl::extrahelp MoreHelp("\nMore help text coming soon...\n");
// CL options

// # probably going to retire soon
// Accepts comma-separated string of analyses
llvm::cl::opt<std::string> AnalysesOption(
    "analyses",
    llvm::cl::desc("Comma-separated list of analyses"),
    llvm::cl::cat(CXXLangstatCategory));

// 2 flags:
// --emit-features: analysis stops after writing features to file after reading in .ast
// --emit-statistics: read in JSON with features and compute statistics
// no flag at all: compute features but don't write them to disk, compute
// statistics and emit them
llvm::cl::opt<Stage> PipelineStage(
    llvm::cl::desc("Stage: "),
    llvm::cl::values(
        clEnumValN(emit_features, "emit-features",
            "Reads in C/C++ source or AST files and\n "
            "stops after emitting features found and\n "
            "stores them in a JSON file. For each \n "
            "input file, a file containing the output\n "
            "will be created."),
        clEnumValN(emit_statistics, "emit-statistics",
            "Read in JSON files generated by -emit-features\n "
            "and outputs a single JSON file containing\n "
            "statistics.")),
    llvm::cl::cat(CXXLangstatCategory));

// --in option, all until --out is taken as input files
llvm::cl::list<std::string> InputFilesOption(
    "in",
    llvm::cl::desc("<src0> [... <srcN>], where srcI can be either a C/C++ source "
        "or AST file"),
    llvm::cl::ValueRequired,
    llvm::cl::ZeroOrMore,
    llvm::cl::cat(IOCategory));

// --out option, optional. when used, should give same #args as with --in.
llvm::cl::opt<std::string> OutputFileOption(
    "out",
    llvm::cl::desc("[<json>]. Use this option if you expect there "
        "to be only a single \noutput file. "
        "Notably, use this when -emit-statistics is enabled."),
    llvm::cl::ValueRequired,
    llvm::cl::cat(IOCategory));

llvm::cl::opt<std::string> InputDirOption(
    "indir",
    llvm::cl::desc("<dir>. Use this option if you want to analyze multiple files."),
    llvm::cl::ValueRequired,
    llvm::cl::cat(IOCategory));

llvm::cl::opt<std::string> OutputDirOption(
    "outdir",
    llvm::cl::desc("<dir>. Use this option if you expect there to be multiple "
        "output files, \ne.g. when -emit-features is used on multiple "
        "files or a directory"),
    llvm::cl::ValueRequired,
    llvm::cl::cat(IOCategory));

// what to do with this? some -p option already there by default, but parser fails on it
static llvm::cl::opt<std::string> BuildPath(
    "p",
    llvm::cl::desc("Build path to dir containing compilation database in JSON format."),
    llvm::cl::Optional, llvm::cl::cat(CXXLangstatCategory));

static llvm::cl::opt<unsigned> ParallelismOption(
    "parallel",
    llvm::cl::desc("Number of threads to use for -emit-features."),
    llvm::cl::init(1),
    llvm::cl::Optional,
    llvm::cl::ValueRequired,
    llvm::cl::cat(CXXLangstatCategory));
static llvm::cl::alias ParallelismOptionAlias(
    "j",
    llvm::cl::desc("Alias for -parallel"),
    llvm::cl::aliasopt(ParallelismOption),
    llvm::cl::NotHidden,
    llvm::cl::cat(CXXLangstatCategory));

//-----------------------------------------------------------------------------
bool isSuitableExtension(llvm::StringRef s, Stage Stage){
    if(Stage == emit_features) {
        return s.equals(".cpp") || s.equals(".cc") || s.equals(".cxx") || s.equals(".C")
            || s.equals(".hpp") || s.equals(".hh") || s.equals(".hxx") || s.equals(".H")
            || s.equals(".c++") || s.equals(".h++")
            || s.equals(".c") || s.equals(".h") // C file formats
            || s.equals(".ast"); // AST file
    } else if(Stage == emit_statistics){
        return s.equals(".json");
    }
    return false;
}

// Returns vector of relative paths of interesting files (source & .ast files
// or .json files) containing in dir with name T. function assumes that T
// suffices with an "/", as it should specify a dir.
std::vector<std::string> getFiles(const Twine& T, Stage Stage){
    // http://www.martinbroadhurst.com/list-the-files-in-a-directory-in-c.html
    // Trust me, would prefer to use <filesystem> too, but I'd have to upgrade
    // to macOS 10.15. Might change this to use conditional compilation
    // to enable <filesystem> for OSes that can use it.
    DIR* dirp = opendir(T.str().c_str());
    if(dirp){
        // std::cout << "dir: " << T.str().c_str() << std::endl;
        struct dirent* dp;
        std::vector<std::string> files;
        std::vector<std::string> res;
        while ((dp = readdir(dirp)) != NULL) {
            files.emplace_back(dp->d_name);
        }
        closedir(dirp);
        std::vector<std::string> dirfiles;
        for(auto file : files){
            // Ignore hidden files & dirs
            if(!llvm::StringRef(file).consume_front(".")){
                // File is interesting to us
                if(isSuitableExtension(llvm::sys::path::extension(file), Stage)){
                    res.emplace_back((T + file).str());
                // File might indicate a directory, store it to search thru it later
                } else if(!llvm::sys::path::filename(file).equals(".")
                    && !llvm::sys::path::filename(file).equals("..")) {
                        dirfiles.emplace_back((T + file).str() + "/");
                }
            }
        }
        // Search thru dirs stored before
        for(auto dirfile : dirfiles) {
            auto files = getFiles(dirfile, Stage);
            for(auto file : files)
                res.emplace_back(file);
        }
        return res;
    }
    return {};
}

int ParallelEmitFeatures(std::vector<std::string> InputFiles,
    std::vector<std::string> OutputFiles, Stage s, std::string Analyses,
    std::string BuildPath,
    std::shared_ptr<clang::tooling::CompilationDatabase> db,
    unsigned Jobs){
        if(Jobs == 1)
            return CXXLangstatMain(InputFiles, OutputFiles, PipelineStage,
                Analyses, BuildPath, db);

        // Function assumes InputFiles.size = OutputFiles.size
        unsigned Files = InputFiles.size();
        if(Jobs > Files)
            Jobs = Files;
        unsigned WorkPerJob = Files/Jobs;
        unsigned JobsWith1Excess = Files%Jobs;
        unsigned b = 0;
        // Need to store futures from std::async in vector, otherwise ~future
        // blocks parallelism.
        // https://stackoverflow.com/questions/36920579/how-to-use-stdasync-to-call-a-function-in-a-mutex-protected-loop
        std::vector<std::future<int>> futures;
        for(unsigned idx=0; idx<Jobs; idx++){
            unsigned Work = WorkPerJob;
            if(idx < JobsWith1Excess){
                Work++;
            }
            std::cout << b << ", " << b+Work << std::endl;
            std::vector<std::string> In(InputFiles.begin() + b,
                InputFiles.begin() + b + Work);
            std::vector<std::string> Out(OutputFiles.begin() + b,
                OutputFiles.begin() + b + Work);
            b+=Work;
            auto r = std::async(std::launch::async, CXXLangstatMain, In,
                Out, s, Analyses, BuildPath, db); // pipeline stage was the problem for async, probably because still 'unparsed'
            futures.emplace_back(std::move(r));
        }
        return 0;
    }

//-----------------------------------------------------------------------------
//
int main(int argc, char** argv){
    // Common parser for command line options, provided by llvm
    // CommonOptionsParser Parser(argc, argv, CXXLangstatCategory);
    // const std::vector<std::string>& spl = Parser.getSourcePathList();
    // CompilationDatabase& db = Parser.getCompilations();
    // I don't like the way input/source files are handled by COP, so I roll
    // my own stuff.
    // Is this good use of shared_ptr?
    std::shared_ptr<CompilationDatabase> db = nullptr;
    // First try to get string of cxxflags after '--', as loadFromCommandLine requires
    std::string ErrorMessage;
    db = FixedCompilationDatabase::loadFromCommandLine(argc, argv, ErrorMessage);
    if(db) {
        std::cout << "COMPILE COMMAND: ";
        for(auto cc : db->getCompileCommands("")){
            for(auto s : cc.CommandLine)
                std::cout << s << " ";
            std::cout << std::endl;
        }
    } else {
        std::cout << "Could not load compile command from command line \n" +
            ErrorMessage;
    }


    // Only now can call this method, otherwise compile command could be interpreted
    // as input or output file since those are positional
    // This usage is encouraged this way according to
    // https://clang.llvm.org/doxygen/classclang_1_1tooling_1_1FixedCompilationDatabase.html#a1443b7812e6ffb5ea499c0e880de75fc
    llvm::cl::ParseCommandLineOptions(argc, argv, "cxx-langstat is a clang-based"
     "tool for computing statistics on C/C++ code on the clang AST level");

    std::vector<std::string> InputFiles;
    std::vector<std::string> OutputFiles;


    bool Files = !InputFilesOption.empty();
    bool Dir = !InputDirOption.empty();
    if(Files && Dir){
        std::cout << "Don't specify both input files and directory "
            "at the same time\n";
        exit(1);
    }

    // Ensure dirs end with "/"
    if(!InputDirOption.empty() && !StringRef(InputDirOption).consume_back("/"))
        InputDirOption += "/";
    if(!OutputDirOption.empty() && !StringRef(OutputDirOption).consume_back("/"))
        OutputDirOption += "/";

    if(Files){
        InputFiles = InputFilesOption;
    } else {
        InputFiles = getFiles(InputDirOption, PipelineStage);
        std::sort(InputFiles.begin(), InputFiles.end());
    }


    // When multiple output files are a fact (multiple input files) or very
    // likely (input dir), require OutputDirOption instead of OutputFileOption to be used.
    // OutputFiles can only be used when it is guaranteed to be only a single output file.
    // emit-features creates one output per input.
    if(PipelineStage == emit_features){
        if(Files && InputFiles.size() == 1){  // single file
            if(!OutputDirOption.empty()){ // may not specify output dir
                exit(1);
            }
            if(!OutputFileOption.empty()){ // place at output file specified
                OutputFiles.emplace_back(OutputFileOption);
            } else { // create output file if none specified
                StringRef filename = llvm::sys::path::filename(InputFiles[0]);
                OutputFiles.emplace_back(filename.str() + ".json");
            }
        } else if((Files && InputFiles.size() > 1) || Dir) { // multiple files
                if(!OutputFileOption.empty()){ // may not specify output file
                    exit(1);
                }
                if(OutputDirOption.empty()){ // obliged to specify output dir
                    exit(1);
                } else {
                    for(const auto& File : InputFiles){ // place at output dir specified
                        StringRef filename = llvm::sys::path::filename(File);
                        OutputFiles.emplace_back(OutputDirOption + filename.str() + ".json");
                    }
                }
            }
        assert(InputFiles.size() == OutputFiles.size());
    // When -emit-features option is not used, only zero or one output file is ok.
    // Output dir is not ok, since the output is guaranteed to be only a single file.
    } else {
        if(!OutputDirOption.empty())
            exit(1);
        if(OutputFileOption.empty()){
            OutputFiles.emplace_back("./stats.json");
        }
        if(!OutputFileOption.empty())
            OutputFiles.emplace_back(OutputFileOption);
        assert(OutputFiles.size() == 1);
    }

    std::cout << "input files(" << InputFiles.size() << "): ";
    for(const auto& InputFile : InputFiles){
        std::cout << InputFile << " ";
        if(StringRef(InputFile).consume_back("/")){
            std::cout << "Specified input dir, quitting.. \n";
            exit(1);
        }
    }
    std::cout << '\n';
    std::cout << "output files(" << OutputFiles.size() << "): ";
    for(const auto& OutputFile : OutputFiles){
        std::cout << OutputFile << " ";
        if(StringRef(OutputFile).consume_back("/")){
            std::cout << "Specified output dir, quitting.. \n";
            exit(1);
        }
    }
    std::cout << '\n';
    // CXXLangstatMain(InputFiles, OutputFiles,
    //     PipelineStage, AnalysesOption, BuildPath, std::move(db));
    if(PipelineStage == emit_features){
        ParallelEmitFeatures(InputFiles, OutputFiles, PipelineStage,
            AnalysesOption, BuildPath, db, ParallelismOption);
    } else if(PipelineStage == emit_statistics){
        return CXXLangstatMain(InputFiles, OutputFiles, PipelineStage,
            AnalysesOption, BuildPath, db);
    }
    return 0;
}
