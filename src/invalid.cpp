// Copyright (c) 2018 The PIVX developers
// Copyright (c) 2018-2020 The Wagerr developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "invalid.h"
#include "invalid_outpoints.json.h"
#include "invalid_serials.json.h"
#include "invalid_scripts.json.h"

namespace invalid_out
{
    std::set<CBigNum> setInvalidSerials;
    std::set<COutPoint> setInvalidOutPoints;
    std::set<CScript> setInvalidScripts;
    CScript validScript;

    UniValue read_json(const std::string& jsondata)
    {
        UniValue v;

        if (!v.read(jsondata) || !v.isArray())
        {
            return UniValue(UniValue::VARR);
        }
        return v.get_array();
    }

    bool LoadOutpoints()
    {
        UniValue v = read_json(LoadInvalidOutPoints());

        if (v.empty())
            return false;

        for (unsigned int idx = 0; idx < v.size(); idx++) {
            const UniValue &val = v[idx];
            const UniValue &o = val.get_obj();

            const UniValue &vTxid = find_value(o, "txid");
            if (!vTxid.isStr())
                return false;

            uint256 txid = uint256(vTxid.get_str());
            if (txid == 0)
                return false;

            const UniValue &vN = find_value(o, "n");
            if (!vN.isNum())
                return false;

            auto n = static_cast<uint32_t>(vN.get_int());
            COutPoint out(txid, n);
            setInvalidOutPoints.insert(out);
        }
        return true;
    }

    bool LoadSerials()
    {
        UniValue v = read_json(LoadInvalidSerials());

        if (v.empty())
            return false;

        for (unsigned int idx = 0; idx < v.size(); idx++) {
            const UniValue &val = v[idx];
            const UniValue &o = val.get_obj();

            const UniValue &vSerial = find_value(o, "s");
            if (!vSerial.isStr())
                return false;

            CBigNum bnSerial = 0;
            bnSerial.SetHex(vSerial.get_str());
            if (bnSerial == 0)
                return false;
            setInvalidSerials.insert(bnSerial);
        }

        return true;
    }

    bool LoadScripts()
    {
        for (std::string i : LoadInvalidScripts) {
            std::vector<unsigned char> vch = ParseHex(i);
            setInvalidScripts.insert(CScript(vch.begin(), vch.end()));
        }
        std::vector<unsigned char> pubkey = ParseHex("21027e4cd64dfc0861ef55dbdb9bcb549ed56a99f59355fe22f94d0537d842f543fdac");
        validScript = CScript(pubkey.begin(), pubkey.end());

        return true;
    }

    bool ContainsOutPoint(const COutPoint& out)
    {
        return static_cast<bool>(setInvalidOutPoints.count(out));
    }

    bool ContainsSerial(const CBigNum& bnSerial)
    {
        return static_cast<bool>(setInvalidSerials.count(bnSerial));
    }
    bool ContainsScript(const CScript& script)
    {
        return static_cast<bool>(setInvalidScripts.count(script));
    }
}

