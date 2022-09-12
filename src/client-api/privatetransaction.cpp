// Copyright (c) 2018 Tadhg Riordan Zcoin Developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validation.h"
#include "client-api/server.h"
#include "rpc/server.h"
#include "util.h"
#include "client-api/wallet.h"
#include "wallet/wallet.h"
#include "wallet/walletexcept.h"
#include "base58.h"
#include "client-api/send.h"
#include "client-api/protocol.h"
#include "wallet/coincontrol.h"
#include "lelantus.h"
#include <sigma.h>
#include <vector>

#include "univalue.h"

UniValue lelantusTxFee(Type type, const UniValue& data, const UniValue& auth, bool fHelp) {
    CAmount nAmount = data["amount"].get_int64();
    bool fSubtractFeeFromAmount = data["subtractFeeFromAmount"].get_bool();

    CCoinControl coinControl;
    GetCoinControl(data, coinControl);

    CCoinControl *ccp = coinControl.HasSelected() ? &coinControl : NULL;

    // payTxFee is a global variable that will be used to estimate the fee.
    payTxFee = CFeeRate(data["feePerKb"].get_int64());

    std::list<CSigmaEntry> sigmaCoins = pwalletMain->GetAvailableCoins(ccp, false, true);
    std::list<CLelantusEntry> lelantusCoins = pwalletMain->GetAvailableLelantusCoins(ccp, false, true);
    std::pair<CAmount, unsigned int> txFeeAndSize = pwalletMain->EstimateJoinSplitFee(nAmount, fSubtractFeeFromAmount, sigmaCoins, lelantusCoins, ccp);
    return txFeeAndSize.first;
}

UniValue sendLelantus(Type type, const UniValue& data, const UniValue& auth, bool fHelp) {
    if (type != Create) {
        throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
    }

    CBitcoinAddress address = find_value(data, "recipient").get_str();
    CAmount amount = find_value(data, "amount").get_int64();

    if (!address.IsValid()) throw JSONAPIError(API_INVALID_REQUEST, "invalid address");
    if (!amount) throw JSONAPIError(API_INVALID_REQUEST, "amount must be greater than 0");

    CCoinControl coinControl;
    bool fHasCoinControl = GetCoinControl(data, coinControl);

    // payTxFee is a global variable that will be used in CreateLelantusJoinSplitTransaction.
    payTxFee = CFeeRate(data["feePerKb"].get_int64());

    bool fSubtractFeeFromAmount = find_value(data, "subtractFeeFromAmount").get_bool();
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    CRecipient recipient = {scriptPubKey, amount, fSubtractFeeFromAmount};
    std::vector<CRecipient> recipients = {recipient};

    std::vector<CAmount> amounts = {amount};

    try {
        CAmount fee = 0;
        std::vector<CAmount> newMints;
        std::vector<CLelantusEntry> spendCoins;
        std::vector<CSigmaEntry> sigmaSpendCoins;
        std::vector<CHDMint> mintCoins;

        UniValue retval = UniValue::VOBJ;

        CWalletTx transaction = pwalletMain->CreateLelantusJoinSplitTransaction(
            recipients,
            fee, // clobbered
            newMints, // clobbered
            spendCoins, // clobbered
            sigmaSpendCoins, // clobbered
            mintCoins, // clobbered
            fHasCoinControl ? &coinControl : nullptr
        );

        if (fee > 10000000) {
            throw JSONAPIError(API_INTERNAL_ERROR, "We have produced a transaction with a fee above 1 FIRO. This is almost certainly a bug.");
        }

        if (!pwalletMain->CommitLelantusTransaction(transaction, spendCoins, sigmaSpendCoins, mintCoins)) {
            throw JSONAPIError(API_INTERNAL_ERROR, "The produced transaction was invalid and was not accepted into the mempool.");
        }

        GetMainSignals().WalletTransaction(transaction);

        retval.pushKV("txid", transaction.GetHash().ToString());

        return retval;
    }
    catch (const InsufficientFunds& e) {
       throw JSONAPIError(API_WALLET_INSUFFICIENT_FUNDS, e.what());
    }
    catch (const std::exception& e) {
      throw JSONAPIError(API_WALLET_ERROR, e.what());
    }
}

UniValue autoMintLelantus(Type type, const UniValue& data, const UniValue& auth, bool fHelp) {
    if (type != Create) {
        throw JSONAPIError(API_TYPE_NOT_IMPLEMENTED, "Error: type does not exist for method called, or no type passed where method requires it.");
    }

    // Ensure Lelantus mints is already accepted by network so users will not lost their coins
    // due to other nodes will treat it as garbage data.
    if (!lelantus::IsLelantusAllowed()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Lelantus is not activated yet");
    }

    EnsureWalletIsUnlocked(pwalletMain);
    if (!pwalletMain || !pwalletMain->zwallet) {
        throw JSONRPCError(RPC_WALLET_ERROR, "lelantus mint/joinsplit is not allowed for legacy wallet");
    }

    std::vector<std::pair<CWalletTx, CAmount>> wtxAndFees;
    std::vector<CHDMint> mints;

    UniValue mintTxs = UniValue::VARR;

    std::string strError = pwalletMain->MintAndStoreLelantus(0, wtxAndFees, mints, true);

    if (strError != "" && strError != "Insufficient funds") {
        throw JSONAPIError(RPC_WALLET_ERROR, strError);
    }

    for (std::pair<CWalletTx, CAmount> wtxAndFee: wtxAndFees) {
        CWalletTx tx = wtxAndFee.first;
        GetMainSignals().WalletTransaction(tx);

        mintTxs.push_back(tx.GetHash().GetHex());
    }

    UniValue retval(UniValue::VOBJ);
    retval.push_back(Pair("mints", mintTxs));
    return retval;
}

static const CAPICommand commands[] =
{ //  category               collection            actor (function)          authPort   authPassphrase   warmupOk
  //  ---------------------  ------------          ----------------          --------   --------------   --------
    { "privatetransaction",  "lelantusTxFee",      &lelantusTxFee,           true,      false,           false  },
    { "privatetransaction",  "sendLelantus",       &sendLelantus,            true,      true,            false  },
    { "privatetransaction",  "autoMintLelantus",   &autoMintLelantus,        true,      true,            false  }
};
void RegisterSigmaAPICommands(CAPITable &tableAPI)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableAPI.appendCommand(commands[vcidx].collection, &commands[vcidx]);
}
