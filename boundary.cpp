#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Analysis/BranchProbabilityInfo.h>
#include <llvm/Analysis/BlockFrequencyInfo.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <numeric>
#include <set>
#include <map>
#include <optional>
#include <vector>
#include <llvm/Analysis/ModuleSummaryAnalysis.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Analysis/ProfileSummaryInfo.h>
#include <llvm/IR/Dominators.h>

#include "boundary.h"

static std::set<std::string> DefaultIoFunctionNames{
        "accept", "accept4",
        "access",
        "bdflush",
        "chdir", "chmod", "chown", "chown32", "chroot",
        "close",
        "connect",
        "creat",
        "epoll_create", "epoll_create1", "epoll_ctl", "epoll_pwait", "epoll_pwait2", "epoll_wait",
        "eventfd", "eventfd2",
        "faccessat", "faccessat2", "fadvise64", "fadvise64_64", "fallocate",
        "readv", "writev", "preadv", "pwritev", "preadv2", "pwritev2", "read", "write", "open", "fread", "fwrite",
        "fopen",
        "_ZNKSt9basic_iosIcSt11char_traitsIcEE5rdbufEv",
        "_ZNKSt3__19basic_iosIwNS_11char_traitsIwEEE5rdbufB8ne200000Ev",
        "_ZNKSt3__19basic_iosIcNS_11char_traitsIcEEE5rdbufB8ne200000Ev",
        "_ZNKSt3__18ios_base5rdbufB8ne200000Ev",
        "sprintf", "printf", "fprintf",
};

namespace AnalysisDirection {
    enum AnalysisDirection {
        BFS, DFS, COMPLEXITY
    };
}

static llvm::cl::list<std::string> LoadProfilesFrom("load-function-profiles", llvm::cl::ZeroOrMore,
                                                    llvm::cl::desc("Files containing function profiles"));

static llvm::cl::list<std::string> IOPasses("boundary-pass", llvm::cl::ZeroOrMore,
                                            llvm::cl::desc("Algorithm for call graph analysis (bfs or dfs)"));

static llvm::cl::opt<AnalysisDirection::AnalysisDirection> BoundaryAnalysisDirection("boundary-analysis-direction",
                                                                                     llvm::cl::init(
                                                                                             AnalysisDirection::COMPLEXITY),
                                                                                     llvm::cl::values(
                                                                                             clEnumValN(
                                                                                                     AnalysisDirection::BFS,
                                                                                                     "bfs", "BFS"),
                                                                                             clEnumValN(
                                                                                                     AnalysisDirection::DFS,
                                                                                                     "dfs", "DFS"),
                                                                                             clEnumValN(
                                                                                                     AnalysisDirection::COMPLEXITY,
                                                                                                     "complexity",
                                                                                                     "extended DFS")
                                                                                     ),
                                                                                     llvm::cl::desc(
                                                                                             "Algorithm for call graph analysis")
);

static llvm::cl::opt<std::string> ModeDumpDir("mode-dump-dir", llvm::cl::init("function_modes"),
                                              llvm::cl::desc(
                                                      "Root directory to dump function execution mode data in (when one of dump* passes is enabled)"));

static llvm::cl::opt<std::string> EnterIoIntrinsic("intrinsic-io-enter", llvm::cl::init(""),
                                                   llvm::cl::desc(
                                                           "Set \"enter io block\" intrinsic name (if empty, intrinsic wont be emitted)"));

static llvm::cl::opt<std::string> ExitIoIntrinsic("intrinsic-io-exit", llvm::cl::init(""),
                                                  llvm::cl::desc(
                                                          "Set \"exit io block\" intrinsic name (if empty, intrinsic wont be emitted)"));

static llvm::cl::opt<std::string> EnterMemoryIntrinsic("intrinsic-memory-enter", llvm::cl::init(""),
                                                       llvm::cl::desc(
                                                               "Set \"enter memory block\" intrinsic name (if empty, intrinsic wont be emitted)"));

static llvm::cl::opt<std::string> ExitMemoryIntrinsic("intrinsic-memory-exit", llvm::cl::init(""),
                                                      llvm::cl::desc(
                                                              "Set \"exit memory block\" intrinsic name (if empty, intrinsic wont be emitted)"));

static llvm::cl::opt<unsigned> InstrumentationComplexityThreshold("complexity-threshold", llvm::cl::init(100),
                                                      llvm::cl::desc("Set minimum function complexity for instrumentation. Boundary won't insert any intrinsics in functions with lower complexity"));

#ifdef D_LINK_TIME_DUMPER
constexpr bool LINK_TIME_DUMPER = true;
#else
constexpr bool LINK_TIME_DUMPER = false;
#endif

#ifdef D_PRINT_ALL_FUNCTIONS
constexpr bool PRINT_ALL_FUNCTIONS = true;
#else
constexpr bool PRINT_ALL_FUNCTIONS = false;
#endif

// Pretty-prints the result of this analysis
static void printStaticCCResult(llvm::raw_ostream &OutS,
                                const ResultIoComplexity &IOCalls);

struct LightNode {
    std::vector<int> Children;
};

static std::set<int> bfsBinary(const std::map<int, LightNode> &InverseCallGraph, const std::set<int> &InitialRoots) {
    std::set<int> WasReached;
    std::set<int> Front = InitialRoots;
    std::set<int> SwapFront;

    while (!Front.empty()) {
        for (auto Id: Front) {
            WasReached.insert(Id);
            for (auto Child: InverseCallGraph.at(Id).Children) {
                if (WasReached.count(Child) == 0 && Front.count(Child) == 0) {
                    SwapFront.insert(Child);
                }
            }
        }
        std::swap(SwapFront, Front);
        SwapFront.clear();
    }

    return WasReached;
}

static std::set<llvm::Function *> bfsBinary(llvm::Module &Module, const std::map<llvm::Function *, int> &FuncMapping,
                                            const std::map<int, LightNode> &InverseCallGraph,
                                            std::set<std::string> &InitialRoots) {
    std::set<int> InitialRootIds;
    for (auto &Func: Module) {
        if (InitialRoots.count(Func.getName().str()) != 0) {
            InitialRootIds.insert(FuncMapping.at(&Func));
        }
    }

    auto IdRes = bfsBinary(InverseCallGraph, InitialRootIds);
    std::set<llvm::Function *> Res;
    for (auto &Func: Module) {
        if (IdRes.count(FuncMapping.at(&Func)) != 0) {
            Res.insert(&Func);
        }
    }

    return Res;
}

static std::set<llvm::Function *> bfsBinary(llvm::Module &Module, const std::map<llvm::Function *, int> &FuncMapping,
                                            std::set<std::string> &InitialRoots) {
    std::map<int, LightNode> InverseCallGraph;
    for (auto &Func: Module) {
        InverseCallGraph[FuncMapping.at(&Func)]; // Force node creation
        int Id = FuncMapping.at(&Func);

        for (auto &BB: Func) {
            for (auto &Ins: BB) {

                // If this is a call instruction then CB will be not null.
                auto *CB = llvm::dyn_cast<llvm::CallBase>(&Ins);
                if (nullptr == CB) {
                    continue;
                }

                // If CB is a direct function call then DirectInvoc will be not null.
                auto DirectInvoc = CB->getCalledFunction();
                if (nullptr == DirectInvoc) {
                    continue;
                }
                InverseCallGraph[FuncMapping.at(DirectInvoc)].Children.push_back(Id);
            }
        }
    }

    return bfsBinary(Module, FuncMapping, InverseCallGraph, InitialRoots);
}

constexpr FunctionComplexity::FunctionComplexity() : Total(0), Io(0), Memory(0) {}

constexpr FunctionComplexity::FunctionComplexity(unsigned int Total, unsigned int Io)
        : Total(Total), Io(Io), Memory(0) {}

constexpr FunctionComplexity::FunctionComplexity(ComplexityScalarType Total, ComplexityScalarType Io,
                                                 ComplexityScalarType Memory)
        : Total(Total), Io(Io), Memory(Memory) {}

constexpr FunctionComplexity FunctionComplexity::operator+(const FunctionComplexity &other) const {
    return {Total + other.Total, Io + other.Io, Memory + other.Memory};
}

constexpr FunctionComplexity &FunctionComplexity::operator+=(const FunctionComplexity &other) {
    Total += other.Total;
    Io += other.Io;
    Memory += other.Memory;
    return *this;
}

constexpr FunctionComplexity FunctionComplexity::IoAsComplexity(unsigned int IoCalls) {
    return FunctionComplexity(IoCalls > 0 ? IoCalls : 1, IoCalls);
}

constexpr FunctionComplexity FunctionComplexity::IoAsComplexity(ComplexityScalarType IoCalls) {
    return FunctionComplexity(IoCalls > 0 ? IoCalls : 1, IoCalls);
}

constexpr FunctionComplexity operator*(unsigned int scl, const FunctionComplexity &c) {
    return {
            scl * c.Total,
            scl * c.Io,
            scl * c.Memory,
    };
}

static constexpr FunctionComplexity PROC_OP = FunctionComplexity(1.0, 0.0, 0.0);
static constexpr FunctionComplexity MEM_OP = FunctionComplexity(1.0, 0.0, 1.0);
static constexpr FunctionComplexity IO_OP = FunctionComplexity(1.0, 1.0, 0.0);

static constexpr unsigned MUL_COST = 6;
static constexpr unsigned DIV_COST = 20;
static constexpr unsigned BRANCH_COST = 5;

static FunctionComplexity nonCallInstructionCost(const llvm::Instruction &Ins) {
    if (Ins.mayReadOrWriteMemory()) return 1 * MEM_OP;

    if (auto BinOp = llvm::dyn_cast<llvm::BinaryOperator>(&Ins); nullptr != BinOp) {
        switch (BinOp->getOpcode()) {
            case llvm::Instruction::FAdd:
            case llvm::Instruction::Add:
            case llvm::Instruction::FSub:
            case llvm::Instruction::Sub:
            case llvm::Instruction::And:
            case llvm::Instruction::Or:
            case llvm::Instruction::Xor:
            case llvm::Instruction::Shl:
            case llvm::Instruction::LShr:
            case llvm::Instruction::AShr:
                return 1 * PROC_OP;

            case llvm::Instruction::Mul:
            case llvm::Instruction::FMul:
                return MUL_COST * PROC_OP;

            case llvm::Instruction::UDiv:
            case llvm::Instruction::SDiv:
            case llvm::Instruction::FDiv:
            case llvm::Instruction::URem:
            case llvm::Instruction::SRem:
            case llvm::Instruction::FRem:
                return DIV_COST * PROC_OP;

            default:
                throw std::runtime_error("unknown BinOp opcode " + std::to_string(BinOp->getOpcode()));
        }
    } else if (auto CastInst = llvm::dyn_cast<llvm::CastInst>(&Ins); nullptr != CastInst) {
        return 1 * PROC_OP;
    } else if (auto CmpInst = llvm::dyn_cast<llvm::CmpInst>(&Ins); nullptr != CmpInst) {
        return 1 * PROC_OP;
    } else if (auto UnaryOp = llvm::dyn_cast<llvm::UnaryOperator>(&Ins); nullptr != UnaryOp) {
        switch (UnaryOp->getOpcode()) {
            case llvm::Instruction::FNeg:
                return 1 * PROC_OP;

            default:
                throw std::runtime_error("unknown UnaryOp opcode " + std::to_string(UnaryOp->getOpcode()));
        }
    } else if (auto RetIns = llvm::dyn_cast<llvm::ReturnInst>(&Ins); nullptr != RetIns) {
        return 1 * PROC_OP;
    } else if (auto AllocaInst = llvm::dyn_cast<llvm::AllocaInst>(&Ins); nullptr != AllocaInst) {
        return 1 * MEM_OP;
    } else if (auto BranchInst = llvm::dyn_cast<llvm::BranchInst>(&Ins); nullptr != BranchInst) {
        return BRANCH_COST * PROC_OP;
    } else if (auto GetElementPtrInst = llvm::dyn_cast<llvm::GetElementPtrInst>(&Ins); nullptr != GetElementPtrInst) {
        return 1 * MEM_OP;
    } else if (auto SwitchInst = llvm::dyn_cast<llvm::SwitchInst>(&Ins); nullptr != SwitchInst) {
        return static_cast<unsigned int>(std::log2(SwitchInst->getNumCases())) * PROC_OP;
    } else if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(&Ins); nullptr != PHINode) {
        return 0 * PROC_OP;
    } else if (auto SelectInst = llvm::dyn_cast<llvm::SelectInst>(&Ins); nullptr != SelectInst) {
        return 1 * PROC_OP;
    } else if (auto ExtractValueInst = llvm::dyn_cast<llvm::ExtractValueInst>(&Ins); nullptr != ExtractValueInst) {
        return 1 * MEM_OP;
    } else if (auto LandingPadInst = llvm::dyn_cast<llvm::LandingPadInst>(&Ins); nullptr != LandingPadInst) {
        return 0 * PROC_OP;
    } else if (auto UnreachableInst = llvm::dyn_cast<llvm::UnreachableInst>(&Ins); nullptr != UnreachableInst) {
        return 0 * PROC_OP;
    } else if (auto InsertValueInst = llvm::dyn_cast<llvm::InsertValueInst>(&Ins); nullptr != InsertValueInst) {
        return 1 * MEM_OP;
    } else if (auto ResumeInst = llvm::dyn_cast<llvm::ResumeInst>(&Ins); nullptr != ResumeInst) {
        return 1 * PROC_OP;
    } else {
        throw std::runtime_error(std::string("unknown instruction ") + Ins.getOpcodeName());
    }
}

static FunctionComplexity
complexityOf(llvm::Function &Func, std::vector<int> &State, std::vector<FunctionComplexity> &ResVec,
             const std::map<llvm::Function *, int> &FuncMapping,
             const std::map<std::string, FunctionComplexity> &ExternalComplexityInfo, llvm::ProfileSummaryInfo &PSI,
             llvm::FunctionAnalysisManager &FAM) {
    enum {
        STATE_INIT = 0,
        STATE_VISITED = 1,
        STATE_PROCESSING = 2,
    };
    int FuncIdx = FuncMapping.at(&Func);
    if (State[FuncIdx] == STATE_VISITED) {
        return ResVec[FuncIdx];
    } else if (State[FuncIdx] == STATE_PROCESSING) {
        return {};
    } else if (ExternalComplexityInfo.count(Func.getName().str()) > 0) {
        auto &Res = ResVec[FuncIdx];

        Res = ExternalComplexityInfo.at(Func.getName().str());
        State[FuncIdx] = STATE_VISITED;
        return Res;
    }
    State[FuncIdx] = STATE_PROCESSING;
    auto &Res = ResVec[FuncIdx];

    if (!Func.empty()) {
        auto &BPI = FAM.getResult<llvm::BranchProbabilityAnalysis>(Func);
        auto &BFI = FAM.getResult<llvm::BlockFrequencyAnalysis>(Func);
        auto EntryFreq = BFI.getBlockFreq(&Func.getEntryBlock()).getFrequency();
        for (auto &BB: Func) {
            auto Freq = (ComplexityScalarType) BFI.getBlockFreq(&BB).getFrequency() / EntryFreq;
            for (auto &Ins: BB) {
                // If this is a call instruction then CB will be not null.
                if (auto *CB = llvm::dyn_cast<llvm::CallBase>(&Ins); CB != nullptr) {
                    // If CB is a direct function call then DirectInvoc will be not null.
                    auto DirectInvoc = CB->getCalledFunction();
                    if (nullptr == DirectInvoc) {
                        Res.Total += 1;
                        continue;
                    }
                    Res += Freq *
                           complexityOf(*DirectInvoc, State, ResVec, FuncMapping, ExternalComplexityInfo, PSI, FAM);
                } else {
                    Res += Freq * nonCallInstructionCost(Ins);
                }
            }
        }
    }

    if (Res.Total == 0) {
        Res.Total = 1;
    }

    State[FuncIdx] = STATE_VISITED;

    return Res;
}

static ComplexityScalarType dfs(llvm::Function &Func, std::vector<int> &State, std::vector<int> &ResVec,
                                const std::map<llvm::Function *, int> &FuncMapping,
                                const std::map<std::string, FunctionComplexity> &Leaves) {
    enum {
        STATE_INIT = 0,
        STATE_VISITED = 1,
        STATE_PROCESSING = 2,
    };
    if (FuncMapping.count(&Func) == 0) {
        return Leaves.at(Func.getFunction().getName().str()).Total;
    }
    int FuncIdx = FuncMapping.at(&Func);
    if (State[FuncIdx] == STATE_VISITED) {
        return ResVec[FuncIdx];
    } else if (State[FuncIdx] == STATE_PROCESSING) {
        return 0;
    }
    State[FuncIdx] = STATE_PROCESSING;
    ResVec[FuncIdx] = Leaves.count(Func.getFunction().getName().str());

    for (auto &BB: Func) {
        for (auto &Ins: BB) {

            // If this is a call instruction then CB will be not null.
            auto *CB = llvm::dyn_cast<llvm::CallBase>(&Ins);
            if (nullptr == CB) {
                continue;
            }

            // If CB is a direct function call then DirectInvoc will be not null.
            auto DirectInvoc = CB->getCalledFunction();
            if (nullptr == DirectInvoc) {
                continue;
            }
            ResVec[FuncIdx] += dfs(*DirectInvoc, State, ResVec, FuncMapping, Leaves);
        }
    }

    State[FuncIdx] = STATE_VISITED;

    return ResVec[FuncIdx];
}

static void
LoadFunctionProfiles(std::map<std::string, FunctionComplexity> &OutComplexityInfo) {
    for (auto &Path: LoadProfilesFrom) {
        std::ifstream In(Path);
        if (!In) {
            throw std::runtime_error("Cannot open function profile file at " + Path);
        }
        std::string Name;
        std::string Mode;

        while (In) {
            ComplexityScalarType Total, Io, Memory;
            In >> Name >> Total >> Io >> Memory;
            if (!In) break;

            OutComplexityInfo[Name] = FunctionComplexity(Total, Io, Memory);
        }
    }
}

BoundaryAnalysis::Result BoundaryAnalysis::runOnModule(llvm::Module &Module, llvm::ModuleAnalysisManager &MAM) {
    auto &FAM = MAM.getResult<llvm::FunctionAnalysisManagerModuleProxy>(Module).getManager();
    auto &PSI = MAM.getResult<llvm::ProfileSummaryAnalysis>(Module);

    llvm::MapVector<const llvm::Function *, FunctionComplexity> Res;
    std::map<llvm::Function *, int> FuncMapping;
    for (auto &Func: Module) {
        if constexpr (PRINT_ALL_FUNCTIONS) {
            llvm::errs() << Func.getName() << "\n";
        }
        auto idx = FuncMapping.size();
        FuncMapping[&Func] = idx;
    }
    if (BoundaryAnalysisDirection == AnalysisDirection::DFS) {
        std::vector<int> State(FuncMapping.size());
        std::vector<int> ResVec(FuncMapping.size());
        for (auto &Func: Module) {
            int SearchRes = dfs(Func, State, ResVec, FuncMapping, ExternalComplexityInfo);
            Res.insert(std::make_pair(&Func, FunctionComplexity::IoAsComplexity((ComplexityScalarType) SearchRes)));
        }
    } else if (BoundaryAnalysisDirection == AnalysisDirection::BFS) {
        std::set<std::string> InitialRoots;
        for (auto &[k, v]: ExternalComplexityInfo) {
            if (v.Io > 0) {
                InitialRoots.insert(k);
            }
        }
        auto SearchRes = bfsBinary(Module, FuncMapping, InitialRoots);
        for (auto F: SearchRes) {
            Res[F] = FunctionComplexity::IoAsComplexity((ComplexityScalarType) 1);
        }
    } else if (BoundaryAnalysisDirection == AnalysisDirection::COMPLEXITY) {
        std::vector<int> State(FuncMapping.size());
        std::vector<FunctionComplexity> ResVec(FuncMapping.size());
        for (auto &Func: Module) {
            auto res = complexityOf(Func, State, ResVec, FuncMapping, ExternalComplexityInfo, PSI, FAM);
            Res.insert(std::make_pair(&Func, res));
        }
    } else {
        llvm::errs() << "Illegal BoundaryAnalysisDirection: " << BoundaryAnalysisDirection << "\n";
        throw std::runtime_error("Illegal BoundaryAnalysisDirection: " + std::to_string(BoundaryAnalysisDirection));
    }

    return Res;
}

llvm::PreservedAnalyses
BoundaryAnalysisPrinter::run(llvm::Module &M,
                             llvm::ModuleAnalysisManager &MAM) {

    auto IOCalls = MAM.getResult<BoundaryAnalysis>(M);

    printStaticCCResult(OS, IOCalls);
    return llvm::PreservedAnalyses::all();
}

void BoundaryAnalysisPrinter::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
}

BoundaryAnalysis::Result
BoundaryAnalysis::run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {
    return runOnModule(M, MAM);
}

void BoundaryAnalysis::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.addRequiredID(llvm::BranchProbabilityAnalysis::ID());
    AU.addRequiredID(llvm::BlockFrequencyAnalysis::ID());
    AU.addRequiredID(llvm::ProfileSummaryAnalysis::ID());
    AU.addRequiredID(llvm::FunctionAnalysisManagerModuleProxy::ID());
    AU.addRequiredID(llvm::DominatorTreeAnalysis::ID());
    AU.setPreservesAll();
}

//------------------------------------------------------------------------------
// New PM Registration
//------------------------------------------------------------------------------
llvm::AnalysisKey BoundaryAnalysis::Key;

llvm::PassPluginLibraryInfo getBoundaryPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "boundary", LLVM_VERSION_STRING,
            [](llvm::PassBuilder &PB) {
                // for "opt -passes=<PASS_NAME>" syntax
                PB.registerPipelineParsingCallback(
                        [&](llvm::StringRef Name, llvm::ModulePassManager &MPM,
                            llvm::ArrayRef<llvm::PassBuilder::PipelineElement> Arr) {
                            if (Name == "print<boundary>") {
                                MPM.addPass(BoundaryAnalysisPrinter(llvm::errs()));
                                return true;
                            } else if (Name == "dump<boundary>") {
                                MPM.addPass(BoundaryAnalysisDumper(ModeDumpDir));
                                return true;
                            } else if (Name == "instrument<boundary>") {
                                MPM.addPass(BoundaryInstrument());
                                return true;
                            }
                            return false;
                        });

                // ld.lld has issues with passing options to loaded passes, so link-time passes behaviour should be defined at compile-time
                if constexpr (LINK_TIME_DUMPER) {
                    PB.registerFullLinkTimeOptimizationEarlyEPCallback(
                            [](llvm::ModulePassManager &MPM, llvm::OptimizationLevel) {
                                MPM.addPass(BoundaryAnalysisDumper(ModeDumpDir));
                            });
                }
                llvm::FunctionAnalysisManager FAM;
                FAM.registerPass([] { return llvm::BranchProbabilityAnalysis(); });
                FAM.registerPass([] { return llvm::BlockFrequencyAnalysis(); });
                PB.registerFunctionAnalyses(FAM);
                llvm::ModuleAnalysisManager MAM;
                MAM.registerPass([&FAM] { return llvm::FunctionAnalysisManagerModuleProxy(FAM); });
                PB.registerModuleAnalyses(MAM);

                PB.registerOptimizerLastEPCallback([](llvm::ModulePassManager &MPM, llvm::OptimizationLevel) {
                    if (std::count(IOPasses.begin(), IOPasses.end(), "print") != 0) {
                        MPM.addPass(BoundaryAnalysisPrinter(llvm::errs()));
                    }
                    if (std::count(IOPasses.begin(), IOPasses.end(), "dump") != 0) {
                        MPM.addPass(BoundaryAnalysisDumper(ModeDumpDir));
                    }
                    if (std::count(IOPasses.begin(), IOPasses.end(), "instrument") != 0) {
                        MPM.addPass(BoundaryInstrument());
                    }
                });
                // for "MAM.getResult<<ANALYSIS_NAME>>(Module)" syntax
                PB.registerAnalysisRegistrationCallback(
                        [](llvm::ModuleAnalysisManager &MAM) {
                            std::map<std::string, FunctionComplexity> ExternalComplexityInfo;
                            for (auto IoFn: DefaultIoFunctionNames) {
                                ExternalComplexityInfo[IoFn] = FunctionComplexity::IoAsComplexity(
                                        (ComplexityScalarType) 1);
                            }
                            LoadFunctionProfiles(ExternalComplexityInfo);
                            MAM.registerPass(
                                    [&] {
                                        return BoundaryAnalysis(ExternalComplexityInfo);
                                    });
                        });
            }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
    return getBoundaryPluginInfo();
}

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
static void printStaticCCResult(llvm::raw_ostream &OutS,
                                const ResultIoComplexity &IOCalls) {
    OutS << std::string(123, '=') << "\n";
    OutS << "IO calls analysis\n";
    OutS << std::string(123, '=') << "\n";
    const char *str1 = "Name";
    const char *str2 = "IO compl";
    const char *str3 = "Memory compl";
    const char *str4 = "Total compl";
    OutS << llvm::format("%-60s %20s %20s %20s\n", str1, str2, str3, str4);
    OutS << std::string(123, '-') << "\n";

    for (auto &CallCount: IOCalls) {
        OutS << llvm::format("%-60s %20llf %20llf %20llf\n", CallCount.first->getName().str().c_str(),
                             CallCount.second.Io, CallCount.second.Memory, CallCount.second.Total);
    }

    OutS << std::string(123, '-') << "\n\n";
}

static bool isReturningToCaller(const llvm::Instruction *Inst) {
    if (llvm::isa<llvm::ReturnInst>(Inst) || llvm::isa<llvm::ResumeInst>(Inst)) {
        return true;
    } else if (auto CleanupRet = llvm::dyn_cast<llvm::CleanupReturnInst>(Inst); CleanupRet != nullptr) {
        return CleanupRet->unwindsToCaller();
    } else {
        return false;
    }
}

static void instrumentFunction(llvm::Function &Func, const std::optional<llvm::FunctionCallee> &EnterMarker,
                               const std::optional<llvm::FunctionCallee> &ExitMarker) {
    auto &Entry = Func.getEntryBlock();
    if (EnterMarker) {
        llvm::CallInst::Create(EnterMarker.value(), {}, llvm::Twine(), &*Entry.begin());
    }
    if (ExitMarker) {
        for (auto &BB: Func) {
            auto Terminator = BB.getTerminator();
            if (isReturningToCaller(Terminator)) {
                llvm::CallInst::Create(ExitMarker.value(), {}, llvm::Twine(), Terminator);
            }
        }
    }
}

llvm::PreservedAnalyses BoundaryInstrument::run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {
    auto &Data = MAM.getResult<BoundaryAnalysis>(M);
    auto EnterIoMarker = EnterIoIntrinsic.empty() ? std::nullopt : std::make_optional(
            M.getOrInsertFunction(llvm::StringRef(EnterIoIntrinsic),
                                  llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), false)));
    auto ExitIoMarker = ExitIoIntrinsic.empty() ? std::nullopt : std::make_optional(
            M.getOrInsertFunction(llvm::StringRef(ExitIoIntrinsic),
                                  llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), false)));
    auto EnterMemoryMarker = EnterMemoryIntrinsic.empty() ? std::nullopt : std::make_optional(
            M.getOrInsertFunction(llvm::StringRef(EnterMemoryIntrinsic),
                                  llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), false)));
    auto ExitMemoryMarker = ExitMemoryIntrinsic.empty() ? std::nullopt : std::make_optional(
            M.getOrInsertFunction(llvm::StringRef(ExitMemoryIntrinsic),
                                  llvm::FunctionType::get(llvm::Type::getVoidTy(M.getContext()), false)));

    unsigned Threshold = InstrumentationComplexityThreshold;

    if (!EnterIoMarker && !ExitIoMarker && !EnterMemoryMarker && !ExitIoMarker) {
        return llvm::PreservedAnalyses::all();
    }

    std::set<std::string> Intrinsics;
    if (!EnterIoIntrinsic.empty()) Intrinsics.insert(EnterIoIntrinsic);
    if (!ExitIoIntrinsic.empty()) Intrinsics.insert(ExitIoIntrinsic);
    if (!EnterMemoryIntrinsic.empty()) Intrinsics.insert(EnterMemoryIntrinsic);
    if (!ExitMemoryIntrinsic.empty()) Intrinsics.insert(ExitMemoryIntrinsic);

    for (auto &Func: M) {
        // External function, body is not available
        if (Func.empty()) continue;

        // Intrinsic, do not modify
        if (Intrinsics.count(Func.getName().str()) != 0) continue;

        FunctionComplexity &Complexity = Data[&Func];
        if (Complexity.Total < Threshold) continue;

        if (Complexity.Io >= Complexity.Total / 8 && (EnterIoMarker || ExitIoMarker)) {
            instrumentFunction(Func, EnterIoMarker, ExitIoMarker);
        } else if (Complexity.Memory >= Complexity.Total / 2 && (EnterMemoryMarker || EnterMemoryMarker)) {
            instrumentFunction(Func, EnterMemoryMarker, ExitMemoryMarker);
        }
    }
    return llvm::PreservedAnalyses::none();
}

void BoundaryInstrument::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.addRequiredID(BoundaryAnalysis::ID());
}

llvm::PreservedAnalyses BoundaryAnalysisDumper::run(llvm::Module &M, llvm::ModuleAnalysisManager &MAM) {
    std::filesystem::create_directory(OutDirName.str());
    std::ofstream Out(OutDirName.str() + "/" + M.getName().str() + ".txt");
    for (auto &[Func, Complexity]: MAM.getResult<BoundaryAnalysis>(M)) {
        if (Complexity.Io > 0 || Complexity.Memory > 0) {
            Out << Func->getName().str() << " " << Complexity.Total << " " << Complexity.Io << " " << Complexity.Memory
                << "\n";
        }
    }

    return llvm::PreservedAnalyses::all();
}

void BoundaryAnalysisDumper::getAnalysisUsage(llvm::AnalysisUsage &AU) const {
    AU.addRequiredID(BoundaryAnalysis::ID());
}
