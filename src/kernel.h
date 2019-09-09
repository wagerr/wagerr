// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2018 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include "main.h"
#include "stakeinput.h"


// MODIFIER_INTERVAL: time to elapse before new modifier is computed
static const unsigned int MODIFIER_INTERVAL = 60;

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

// Compute the hash modifier for proof-of-stake
bool GetKernelStakeModifier(const uint256& hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake);
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);
uint256 ComputeStakeModifier(const CBlockIndex* pindexPrev, const uint256& kernel);
bool Stake(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx, uint256& hashProofOfStake);
bool StakeV1(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, const uint32_t nTimeBlockFrom, unsigned int nBits, int64_t& nTimeTx, uint256& hashProofOfStake);

// Initialize the stake input object
bool initStakeInput(const CBlock& block, std::unique_ptr<CStakeInput>& stake, int nPreviousBlockHeight);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(const CBlock& block, uint256& hashProofOfStake, std::unique_ptr<CStakeInput>& stake, int nPreviousBlockHeight);
bool CheckStakeKernelHash(const CBlockIndex* pindexPrev, const unsigned int nBits, CStakeInput* stake, const unsigned int nTimeTx, uint256& hashProofOfStake, const bool fVerify = false);
// Returns the proof of stake hash
bool GetHashProofOfStake(const CBlockIndex* pindexPrev, CStakeInput* stake, const unsigned int nTimeTx, const bool fVerify, uint256& hashProofOfStakeRet);
// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);

bool ContextualCheckZerocoinStake(int nPreviousBlockHeight, CStakeInput* stake);

int64_t GetTimeSlot(const int64_t nTime);
int64_t GetCurrentTimeSlot();

#endif // BITCOIN_KERNEL_H
