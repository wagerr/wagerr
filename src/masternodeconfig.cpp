// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018 The Wagerr developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// clang-format off
#include "net.h"
#include "masternodeconfig.h"
#include "util.h"
#include "ui_interface.h"
#include <base58.h>
// clang-format on

CMasternodeConfig masternodeConfig;

void CMasternodeConfig::add(std::string alias, std::string ip, std::string privKey, std::string txHash, std::string outputIndex)
{
    CMasternodeEntry cme(alias, ip, privKey, txHash, outputIndex);
    entries.push_back(cme);
}

bool CMasternodeConfig::read(std::string& strErr)
{
    int linenumber = 1;
    boost::filesystem::path pathMasternodeConfigFile = GetMasternodeConfigFile();
    boost::filesystem::ifstream streamConfig(pathMasternodeConfigFile);

    if (!streamConfig.good()) {
        FILE* configFile = fopen(pathMasternodeConfigFile.string().c_str(), "a");
        if (configFile != NULL) {
            std::string strHeader = "#Multi masternode config\n"
                                    "#=======================\n"
                                    "# \n"
                                    "#The multi masternode config allows you to control multiple masternodes from a single wallet. The wallet needs to have a valid collateral output of 10000 coins for each masternode. To use this, place a file named masternode.conf in the data directory of your install:\n"
                                    "# * Windows: %APPDATA%\wagerr\\n"
                                    "# * Mac OS: ~/Library/Application Support/wagerr/\n"
                                    "# * Unix/Linux: ~/.wagerr/\n"
                                    "# \n"
                                    "#The new masternode.conf format consists of a space seperated text file. Each line consisting of an alias, IP address followed by port, masternode private key, collateral output transaction id, collateral output index, donation address and donation percentage (the latter two are optional and should be in format 'address:percentage').\n"
                                    "# \n"
                                    "#Example:\n"
                                    "#```\n"
                                    "#mn1 127.0.0.2:55002 7gb6HNz8gRwVwKZLMGQ6XEaLjzPoxUNK4ui3Pig6mXA6RZ8xhsn 49012766543cac37369cf3813d6216bdddc1b9a8ed03ac690221be10aa5edd6c 0\n"
                                    "#mn2 127.0.0.3:55002 7gHrUV5JFdKF8cYLxAhfzDsj5RkRzebkHuHTG7pErCgaYGxT2vn 49012766543cac37369cf3813d6216bdddc1b9a8ed03ac690221be10aa5edd6c 1 TKa5kuygkJcMDmwsEP1aRawyXPUjbNKxqJ:33\n"
                                    "#          mn3 127.0.0.4:55002 7hTm4uYFYicJb5WhyV7GDkHXJvmn3JT1EvzYTkf9Tnd7pvXa2NQ fdcf8c644452e0c1faff174e5a08948cb550c42a839d10c7ca2e0992bf77a65a 1 TKa5kuygkJcMDmwsEP1aRawyXPUjbNKxqJ\n"
                                    "#```\n"
                                    "# \n"
                                    "#In the example above:\n"
                                    "#* the collateral for mn1 consists of transaction 49012766543cac37369cf3813d6216bdddc1b9a8ed03ac690221be10aa5edd6c, output index 0 has amount 25000\n"
                                    "#* masternode 2 will donate 33% of its income\n"
                                    "#* masternode 3 will donate 100% of its income\n"
                                    "# \n"
                                    "# \n"
                                    "#The following new RPC commands are supported:\n"
                                    "#* list-conf: shows the parsed masternode.conf\n"
                                    "#* start-alias \<alias\>\n"
                                    "#* stop-alias \<alias\>\n"
                                    "#* start-many\n"
                                    "#* stop-many\n"
                                    "#* outputs: list available collateral output transaction ids and corresponding collateral output indexes\n"
                                    "# \n"
                                    "#When using the multi masternode setup, it is advised to run the wallet with 'masternode=0' as it is not needed anymore.\n";
            fwrite(strHeader.c_str(), std::strlen(strHeader.c_str()), 1, configFile);
            fclose(configFile);
        }
        return true; // Nothing to read, so just return
    }

    for (std::string line; std::getline(streamConfig, line); linenumber++) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string comment, alias, ip, privKey, txHash, outputIndex;

        if (iss >> comment) {
            if (comment.at(0) == '#') continue;
            iss.str(line);
            iss.clear();
        }

        if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
            iss.str(line);
            iss.clear();
            if (!(iss >> alias >> ip >> privKey >> txHash >> outputIndex)) {
                strErr = _("Could not parse masternode.conf") + "\n" +
                         strprintf(_("Line: %d"), linenumber) + "\n\"" + line + "\"";
                streamConfig.close();
                return false;
            }
        }

        if (Params().NetworkID() == CBaseChainParams::MAIN) {
            if (CService(ip).GetPort() != 55002) {
                strErr = _("Invalid port detected in masternode.conf") + "\n" +
                         strprintf(_("Line: %d"), linenumber) + "\n\"" + line + "\"" + "\n" +
                         _("(must be 55002 for mainnet)");
                streamConfig.close();
                return false;
            }
        } else if (CService(ip).GetPort() == 55002) {
            strErr = _("Invalid port detected in masternode.conf") + "\n" +
                     strprintf(_("Line: %d"), linenumber) + "\n\"" + line + "\"" + "\n" +
                     _("(55002 could be used only on mainnet)");
            streamConfig.close();
            return false;
        }


        add(alias, ip, privKey, txHash, outputIndex);
    }

    streamConfig.close();
    return true;
}

bool CMasternodeConfig::CMasternodeEntry::castOutputIndex(int &n)
{
    try {
        n = std::stoi(outputIndex);
    } catch (const std::exception e) {
        LogPrintf("%s: %s on getOutputIndex\n", __func__, e.what());
        return false;
    }

    return true;
}
