# cxx-langstat

cxx-langstat is a clang-based tool to analyze the adoption and prevalence of language features in C/C++ codebases.
Leveraging clang's ASTMatchers library we can analyze source code on the AST (abstract syntax tree) level to gain insights about the usage of high-level programming constructs.
By finding the feature/construct in code and counting them, we can achieve insights about the popularity and prevalence of it.

Gaining insights is achieved in two steps, called "stages":
- Emitting features from code: in this stage, the ASTs of the code are considered. By finding all instances of a feature (e.g. all variables that are `constexpr`) we can write a human-readable JSON file that contains all occurrences of a feature that interests us.
- Emitting statistics from features: By using the occurrences from the JSON file from before, we can compute statistics e.g. by counting them.

This separation of the computation of statistics into two steps aids debugging (human-readable features) and avoids recomputation as we can compute new statistics from already extracted features, avoiding reparsing to get the AST or rematching of the AST.

Apart from the analyses that the tool comes with (see below) it also has an API that allows it to register and execute new analyses.

cxx-langstat was developed as a group effort as patr of the Scalable and Parallel Computing Lab at ETH Zurich. 
* Hartogs, Siegfried; [Bachelor Thesis](https://www.research-collection.ethz.ch/handle/20.500.11850/480835).
* Dragancea, Constantin; [Master's Thesis]().


## Instructions
### Requirements
- [LLVM](http://llvm.org) (>=11) including clang and clang-tools-extra. Clang libraries such as libclangBasic, libclangTooling have to be installed. Currently, debian-based distros wil install them as part of clang-tools-extra, while fedora doesn't. You can check by looking for the libraries with `find /usr -iname "libclangbasic.*"`.
- [JSON for Modern C++](https://github.com/nlohmann/json)
- Build system: `ninja` or `make` etc.
- `cmake`

### Building
1. Clone/download cxx-langstat project
2. Download the single-include `json.hpp` from [JSON for Modern C++](https://github.com/nlohmann/json) and put it in `cxx-langstat/include/nlohmann` or use one of the other suggested integration methods
3. `mkdir build && cd build`  
4. `cmake -G "<generator>" -DCMAKE_CXX_COMPILER=<C++ compiler> -DLLVM_DIR=<LLVM cmake dir> -DLLVM_INCLUDE_DIRS=<LLVM include dir> ../`. Example:

```cmake -G "Ninja" -DCMAKE_MAKE_PROGRAM=ninja -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DLLVM_DIR=/usr/lib64/cmake/llvm -DCLANG_DIR=/usr/lib64/cmake/clang/ -DLLVM_INCLUDE_DIRS=/usr/include/llvm ../```

5. `ninja` or `make` to build the binary


### Testing
The [LLVM integrated tester](https://www.llvm.org/docs/CommandGuide/lit.html) is used to test if features are correctly extracted from source/AST files - install using pip: `pip install lit` \
When in build directory,  type `lit test -s`. Use `-vv` to see why individual test cases fail.
### Running
Basic usage: ` cxx-langstat [options] `  
Options:
- `-analyses=<string>` Accepts a string of comma-separated shorthands of the analyses, e.g. `-analyses=msa,ula` will run MoveSemanticsAnalysis and UtilityLibAnalysis.
- `-in`, `-indir`: specify input file or directory
- `-out`, `-outdir`: specify output file or directory
- `-emit-features`: compute features from source or AST files
- `-emit-statistics`: compute statistics from features
- `parallel` or `-j`: number of parallel instances to use, works for `-emit-features` only

#### Example use cases:
##### Single file
To analyze a single source or AST file:
1. Extract features: \
`cxx-langstat -analyses=<> -emit-features -in Helloworld.cpp -out Helloworld.cpp.json `
2. Compute statistics: \
`cxx-langstat -analyses=<> -emit-statistics -in Helloworld.cpp.json -out Helloworld.json`

##### Whole project
To analyze a complete software project, specify the root of it using the `-indir` flag
1. `cxx-langstat -analyses=<> -emit-features -indir MyProject/ -outdir features/` \
will automatically consider ALL code files (.cpp, .h, .ast etc.) because of `-emit-features`
2. `cxx-langstat -analyses=<> -emit-statistics -indir features/ -out stats.json`
will automatically consider ALL .json files because of `-emit-statistics`

Emitting features for a project creates a JSON file for each input file, make sure to place them in a directory created before running it.
Computing statistics creates a single JSON file.

### Adding new analyses
Steps of creating a new analysis:
1. Create YourAnalysis.cpp and YourAnalysis.h
2. In YourAnalysis.h, create header guards: YOURANALYSIS_H etc.
3. Include YourAnalysis.h in YourAnalysis.cpp
4. Add YouAnalysis to CMakeLists.txt
5. Add YourAnalysis to AnalysisRegistry.cpp
6. Implement analyzeFeatures and processFeatures in YourAnalysis.cpp
7. Advice: in YourAnalysis.h, create datastructure that holds features in neat way and implement functions to convert that type to/from JSON

## Implemented Analyses
##### Algorithm Library Analysis (ALA)
Anecdotal evidence suggests that the [C++ Standard Library Algorithms](https://en.cppreference.com/w/cpp/algorithm) are rarely used, motivating analysis to check this claim.
ALA finds and counts calls to function template from the STL algorithms; however, the C++20 `std::ranges` algorithms aren't considered.

##### Constexpr Analysis (CEA)
Finds and counts how often variables, functions and `if`-statements are (not) `constexpr`. This could help us learning how prevalent compile-time constructs are. However, we probably should distinguish between those constructs that aren't `constexpr` and those that can't be. Currently, only some trivial conditions that prohibit `constexpr`-ness are checked, leading me to believe that we will underestimate the popularity of the keyword.  

##### Container Library Analysis (CLA)
CLA reports variable declarations whose type is a [C++ Standard Library Container](https://en.cppreference.com/w/cpp/container):
`array`, `vector`, `forward_list`, `list`, `map`, `multimap`, `set`, `multiset`, `unordered_map`, `unordered_multimap`, `unordered_set`, `unordered_multiset`, `queue`, `priority_queue`, `stack`, `deque`. \
"Variable declarations" include member variables and function parameters. Other occurrences of containers are not respected.

##### Cyclomatic Complexity Analysis (CCA)
For each explicit (not compiler-generated) function declaration that has a body (i.e. is defined), CCA calculates the so-called cyclomatic complexity. This concept developed by Thomas J. McCabe, intuitively, computes for a "section of source code the number of independent paths within it, where linearly-independent means that each path has at least one edge that is not in the other paths." (https://en.wikipedia.org/wiki/Cyclomatic_complexity)

##### Function Parameter Analysis (FPA)
FPA extracts and counts the parameters of functions, function templates and their instantiations and specializations. This gives us insights about the commonness of the different kinds of parameters: by value, non-`const` lvalue ref, `const` lvalue ref, rvalue ref, forwarding ref.

##### Loop Depth Analysis (LDA)
LDA computes the depth of each loops and counts commonness of the dephts. Example of depth 2:
```c++
for(;;){
  do{
  doSomething();
  }while(true);
}
```
Currently the matchers for this analysis grow exponentially with the maximum loop depth to look for, which is not (yet) a problem since depths >5 are rare. Still, switching to a dominator tree-based approach might be favorable.

##### Loop Kind Analysis (LKA)
Extracts and counts`for`, `while`, `do-while` and range-based `for` loops in C++. Especially interesting to investigate the adoption of range-based `for` since C++11.
Limitation: not all occurrences of "traditional" loops can be converted to range-based loops, e.g. infinite loops, which might lead to LKA underestimating the popularity of range-based `for`.

##### Move Semantics Analysis (MSA)
- Counts uses of `std::move`, `std::forward`
- For each type, counts how often by-value parameter at function call sites where constructed by copy/move, respectively.

##### Template Parameter Analysis (TPA)
For the different kinds of templates (class, function, variable, alias), TPA counts the different kinds of template parameters (non-type template, type template and template template parameters) and reports whether the template employs a template parameter pack or not \
(TPA finds *template* parameter packs, but gives no information about *function* parameter packs).

##### Template Instantiation Analysis (TIA)
Reports uses of templated entities and explicit template instantiations. Currently the following class template uses are reported:
* static/non-static member/non-member variable decalaration
* function parameter declarations
* variable and function parameter declarations of pointer and/or reference type: `std::vector<int>* &vec`
* function return types
* epxressions: `return std::vector<int>();`
* new-expressions: `auto* ptr = new std::vector<int>();`

For template functions, the following uses are reported:
* call expressions: `std::sort(...)`
* references: `auto func = &std::sort` (oversimplified example)

Since the data types and function analyzed in ALA, CLA and ULA are mostly templates, they are based on TIA.

##### `using` Analysis (UA)
C++11 introduced type aliases (`using` keyword) which are similar to `typedef`s, but additionally can be templated. This analysis aims to find out if programmers shifted from `typedef`s to aliases. The analysis gives you usage figures of `typedef`s, aliases, "`typedef` templates" (an idiom used to get around above said `typedef` limitation) and alias templates.
<table>
<tr>
<th> Pre-C++11 </th>
<th> Alias </th>
</tr>
<tr>
<td>

  ```c++
  // Regular typedef
  typedef std::vector<int> IntVector;
  // "typedef template" idiom
  template<typename T>
  struct TVector {
    // doesn't have to be named 'type'
    typedef std::vector<T> type;
  };
  ```
</td>
<td>

  ```c++
 // alias
 using IntVector = std::vector<int>;
 // alias template
 template<typename T>
 using TVector = std::vector<T>;
  ```
</td>
</tr>
</table>

##### Utility Library Analysis (ULA)
Similar to the Container Library Analysis; analyzes the usage of certain class template types, namely, the following [C++ Utilities](https://en.cppreference.com/w/cpp/utility): `pair`, `tuple`, `bitset`, `unique_ptr`, `shared_ptr` and `weak_ptr`. \
Interesting to extend to see if `auto_ptr` is still used in C++.

##### Variable Template Analysis (VTA)
C++14 added variable templates. Previously, one used either class templates with a static data member or constexpr function templates returning the desired value. We here analyze whether programmers transitioned in favor of the new concept by reporting usage of the three constructs.
<table>
<tr>
<th> Class template with static member </th>
<th> Constexpr function template </th>
<th> Variable template (since C++14) </th>
</tr>
<tr>
<td>

  ```c++
  template<typename T>
  class Widget {
  public:
      static T data;
  };
  ```
</td>
<td>

  ```c++
  template<typename T>
  constexpr T f1(){
      T data;
      return data;
  }
  ```
</td>

<td>

  ```c++
  template<typename T>
  T data;
  ```
</td>
</tr>
</table>

##### Modern Keywords Analysis (MKA)
This analysis detects the usage of 10 modern (C++11 onwards) [keywords](https://en.cppreference.com/w/cpp/keyword). Reports their appearances and other relevant information. For keywords that refer to specifiers that can be automatically generated by the compiler, it also reports how often the compiler does so.

##### Object Usage Analysis (OUA)
Similar to TIA, reports the usage of non-templated objects: specific classes and functions. It can be restricted based on the object's kind (class vs function), name, and header definition location.

##### Concurrency Tools Analysis (CTA)
It is intended to report and compare the usage of many different tools used in achieving parallel computation: keywords (co_yield, co_await, etc.), standard library tools (async, promise, mutex, etc.), and tools from third party libraries (Boost, Intel TBB).

Currently, the analysis only reports the use of some tools from the standard library: threads, mutexes, promises, and their variants.

## Attributions
- `add_new_analysis.py` and `AnalysisList` are derived from LLVM [clang-tidy](https://github.com/llvm/llvm-project/tree/llvmorg-12.0.0/clang-tools-extra/clang-tidy)'s `add_new_check.py` and `GlobList`, respectively, which are distributed under the [Apache License v2.0 with LLVM Exceptions](https://llvm.org/LICENSE.txt).
- Parts of `Driver.cpp` and the `MatchingExtractor` were learned from and inspired by and the CYC computation in `CyclomaticComplexityAnalysis` was derived from Peter Goldsborough's clang-useful tutorial: [talk](https://cppnow2017.sched.com/event/A8Ij/clang-useful-building-useful-tools-with-llvm-and-clang-for-fun-and-profit), [code](https://github.com/peter-can-talk/cppnow-2017/blob/master/code/mccabe/mccabe.cpp).
- Code to walk through directories used in `Runner.cpp` was copied from [here](http://web.archive.org/web/20201111224934/http://www.martinbroadhurst.com/list-the-files-in-a-directory-in-c.html).
