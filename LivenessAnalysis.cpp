//
// LivenessAnalysis
// Analyses the liveness of physical registers in order to get an unused
// (dead/killed) register when we have the need of a scratch register
//

#include "LivenessAnalysis.h"
#include "../X86.h"
#include "../X86Subtarget.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

using namespace llvm;

ScratchRegTracker::ScratchRegTracker(MachineBasicBlock &MBB) : MBB(MBB) {
  performLivenessAnalysis();
}

void ScratchRegTracker::addReg(MachineInstr &MI, int reg) {
  auto it = regs.find(&MI);
  if (it != regs.end()) {
    it->second.push_back(reg);
  } else {
    std::vector<int> tmp;
    tmp.push_back(reg);
    regs.insert(std::make_pair(&MI, tmp));
  }
}

std::vector<int> *ScratchRegTracker::findRegs(MachineInstr &MI) {
  auto it = regs.find(&MI);
  if (it != regs.end()) {
    std::vector<int> *tmp = &it->second;
    if (tmp->size() > 0) {
      return tmp;
    }
  }
  return nullptr;
}

int ScratchRegTracker::getReg(MachineInstr &MI) {
  std::vector<int> *tmp = findRegs(MI);
  if (tmp)
    return tmp->back();
  return NULL;
}

std::vector<int> *ScratchRegTracker::getRegs(MachineInstr &MI) {
  std::vector<int> *tmp = findRegs(MI);
  if (tmp)
    return tmp;
  return nullptr;
}

int ScratchRegTracker::popReg(MachineInstr &MI) {
  std::vector<int> *tmp = findRegs(MI);
  if (tmp) {
    int retval = tmp->back();
    tmp->pop_back();
    return retval;
  }

  return NULL;
}

int ScratchRegTracker::count(MachineInstr &MI) {
  std::vector<int> *tmp = findRegs(MI);
  if (tmp)
    return tmp->size();
  return 0;
}

void ScratchRegTracker::performLivenessAnalysis() {
  const MachineFunction *MF = MBB.getParent();
  const TargetRegisterInfo &TRI = *MF->getSubtarget().getRegisterInfo();
  const MachineRegisterInfo &MRI = MF->getRegInfo();
  LivePhysRegs LiveRegs(TRI);
  LiveRegs.addLiveOuts(MBB);

  // Data-flow analysis is performed starting from the end of each basic block,
  // iterating each instruction backwards to find USEs and DEFs of each physical
  // register
  for (auto I = MBB.rbegin(); I != MBB.rend(); ++I) {
    MachineInstr *MI = &*I;

    for (unsigned reg : X86::GR32RegClass) {
      if (LiveRegs.available(MRI, reg)) {
        addReg(*MI, reg);
      }
    }

    LiveRegs.stepBackward(*MI);
  }

  dbgs() << "[*] Register liveness analysis performed on basic block "
         << MBB.getNumber() << "\n";
}