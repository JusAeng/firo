// Copyright (c) 2018 Tadhg Riordan, Zcoin Developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCOIN_ZMQ_ZMQNOTIFICATIONINTERFACE_H
#define ZCOIN_ZMQ_ZMQNOTIFICATIONINTERFACE_H

#include "validationinterface.h"
#include "../wallet/wallet.h"
#include <string>
#include <map>
#include <boost/thread/thread.hpp>

class CBlockIndex;
class CZMQAbstract;

class CZMQInterface
{
public:
    bool Initialize();
    void Shutdown(); 

protected:
    std::list<CZMQAbstract*> notifiers;
    boost::thread* worker;
};


class CZMQPublisherInterface : public CValidationInterface, CZMQInterface
{
public:
    CZMQPublisherInterface();
    bool StartWorker();
    virtual ~CZMQPublisherInterface();
    static CZMQPublisherInterface* Create();

protected:
    // CValidationInterface
    void WalletTransaction(const CTransaction& tx);
    void NotifyTransactionLock(const CTransaction& tx);
    void NotifyTxoutLock(COutPoint txout, bool isLocked);
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload);
    void NotifyMasternodeList();
    void NotifyAPIStatus();
    void UpdatedMasternode(CDeterministicMNCPtr masternode);
    void UpdatedSettings(std::string update);
};

class CZMQReplierInterface : public CZMQInterface
{
public:
    CZMQReplierInterface();
    virtual ~CZMQReplierInterface();
    static CZMQReplierInterface* Create();
};

#endif // ZCOIN_ZMQ_ZMQNOTIFICATIONINTERFACE_H
