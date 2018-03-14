// Copyright (c) 2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/zerocoin.h"
#include "hash.h"
#include "util.h"
#include "utilstrencodings.h"

uint256 GetSerialHash(const CBigNum& bnSerial)
{
    uint256 nSerial = bnSerial.getuint256();
    return Hash(nSerial.begin(), nSerial.end());
}

bool CZerocoinMint::GetKeyPair(CKey &key) const
{
    if (version < STAKABLE_VERSION)
        return error("%s: version is %d", __func__, version);

    if (privkey.empty())
        return error("%s: empty privkey %s", __func__, privkey.data());

    return key.SetPrivKey(privkey, true);
}

std::string CZerocoinMint::ToString() const
{
    std::string str = strprintf("version=%d randomness: %s \n serial %s \n privkey %s\n", version, randomness.GetHex(), serialNumber.GetHex(), HexStr(privkey));
    return str;
}

void CZerocoinSpendReceipt::AddSpend(const CZerocoinSpend& spend)
{
    vSpends.emplace_back(spend);
}

std::vector<CZerocoinSpend> CZerocoinSpendReceipt::GetSpends()
{
    return vSpends;
}

void CZerocoinSpendReceipt::SetStatus(std::string strStatus, int nStatus, int nNeededSpends)
{
    strStatusMessage = strStatus;
    this->nStatus = nStatus;
    this->nNeededSpends = nNeededSpends;
}

std::string CZerocoinSpendReceipt::GetStatusMessage()
{
    return strStatusMessage;
}

int CZerocoinSpendReceipt::GetStatus()
{
    return nStatus;
}

int CZerocoinSpendReceipt::GetNeededSpends()
{
    return nNeededSpends;
}
