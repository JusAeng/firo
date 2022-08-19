// Copyright (c) 2018 Tadhg Riordan, Zcoin Developer
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqinterface.h"
#include "zmqpublisher.h"
#include "zmqreplier.h"

#include "version.h"
#include "chainparamsbase.h"
#include "validation.h"
#include "streams.h"
#include "util.h"

void zmqError(const char *str)
{
    LogPrint(NULL, "zmq: Error: %s, errno=%s\n", str, zmq_strerror(errno));
}

// Called at startup to conditionally set up ZMQ socket(s)
bool CZMQInterface::Initialize()
{
    std::list<CZMQAbstract*>::iterator i=notifiers.begin();
    for (; i!=notifiers.end(); ++i)
    {
        CZMQAbstract *notifier = *i;
        if (notifier->Initialize())
        {
            LogPrint(NULL, "  Notifier %s ready (address = %s)\n", notifier->GetType(), notifier->GetAuthority());
        }
        else
        {
            LogPrint(NULL, "  Notifier %s failed (address = %s)\n", notifier->GetType(), notifier->GetAuthority());
            return false;
        }
    }

    if (i!=notifiers.end())
    {
        return false;
    }

    return true;
}

// Called during shutdown sequence
void CZMQInterface::Shutdown()
{
    for (std::list<CZMQAbstract*>::iterator i=notifiers.begin(); i!=notifiers.end(); ++i)
    {
        CZMQAbstract *notifier = *i;
        LogPrint(NULL, "   Shutdown notifier %s at %s\n", notifier->GetType(), notifier->GetAuthority());
        notifier->Shutdown();
    }
}

CZMQReplierInterface::CZMQReplierInterface()
{
}

CZMQReplierInterface::~CZMQReplierInterface()
{
    Shutdown();

    for (std::list<CZMQAbstract*>::iterator i=notifiers.begin(); i!=notifiers.end(); ++i)
    {
        delete *i;
    }
}

CZMQReplierInterface* CZMQReplierInterface::Create()
{
    CZMQReplierInterface* replierInterface = NULL;
    std::map<std::string, CZMQFactory> factories;
    std::list<CZMQAbstract*> notifiers;

    factories["auth"] = CZMQAbstract::Create<CZMQAuthReplier>;
    factories["open"] = CZMQAbstract::Create<CZMQOpenReplier>;

    for (std::map<std::string, CZMQFactory>::const_iterator i=factories.begin(); i!=factories.end(); ++i)
    {
        std::string type = i->first;
        std::string address = BaseParams().APIAddr();
        std::string port = type=="auth" ? std::to_string(BaseParams().APIAuthREPPort()) :
                                     std::to_string(BaseParams().APIOpenREPPort());

        CZMQFactory factory = factories[type];
        CZMQAbstract *notifier = factory();
        notifier->SetType("REP" + type);
        notifier->SetAddress(address);
        notifier->SetPort(port);
        notifier->SetAuthority(address + port);
        notifiers.push_back(notifier);
    }


    replierInterface = new CZMQReplierInterface();
    replierInterface->notifiers = notifiers;

    if (!replierInterface->Initialize())
    {
        delete replierInterface;
        replierInterface = NULL;
    }
    

    LogPrintf("returning CZMQReplierInterface\n");
    return replierInterface;
}


CZMQPublisherInterface::CZMQPublisherInterface()
{
}

bool CZMQPublisherInterface::StartWorker()
{
    // Create worker
    worker = new boost::thread(boost::bind(&CZMQThreadPublisher::Thread));
    return true;
}

CZMQPublisherInterface::~CZMQPublisherInterface()
{
    Shutdown();

    for (std::list<CZMQAbstract*>::iterator i=notifiers.begin(); i!=notifiers.end(); ++i)
    {
        delete *i;
    }

    //destroy worker
    if (worker) worker->interrupt();
}

CZMQPublisherInterface* CZMQPublisherInterface::Create()
{
    LogPrintf("in CreateWithArguments..\n");
    CZMQPublisherInterface* notificationInterface = NULL;
    std::map<std::string, CZMQFactory> factories;
    std::list<CZMQAbstract*> notifiers;

    // Ordering here implies ordering of topic publishing.
    std::vector<std::string> pubIndexes = {
        "pubblock", 
        "pubrawtx",
        "pubmasternodeupdate",
        "pubsettings",
        "pubstatus",
        "pubmasternodelist",
        "publockstatus"
    };

    factories["pubblock"] = CZMQAbstract::Create<CZMQBlockDataTopic>;
    factories["pubrawtx"] = CZMQAbstract::Create<CZMQTransactionTopic>;
    factories["pubmasternodeupdate"] = CZMQAbstract::Create<CZMQMasternodeTopic>;
    factories["pubsettings"] = CZMQAbstract::Create<CZMQSettingsTopic>;
    factories["pubstatus"] = CZMQAbstract::Create<CZMQAPIStatusTopic>;
    factories["pubmasternodelist"] = CZMQAbstract::Create<CZMQMasternodeListTopic>;
    factories["publockstatus"] = CZMQAbstract::Create<CZMQLockStatusTopic>;
    
    BOOST_FOREACH(std::string pubIndex, pubIndexes)
    {
        CZMQFactory factory = factories[pubIndex];
        CZMQAbstract *notifier = factory();
        std::string address = BaseParams().APIAddr();
        std::string port = pubIndex=="pubstatus" ? std::to_string(BaseParams().APIOpenPUBPort()) :
                                           std::to_string(BaseParams().APIAuthPUBPort());
        notifier->SetType("zmq" + pubIndex);
        notifier->SetAddress(address);
        notifier->SetPort(port);
        notifier->SetAuthority(address + port);
        notifiers.push_back(notifier);
    }

    notificationInterface = new CZMQPublisherInterface();
    notificationInterface->notifiers = notifiers;

    if (!notificationInterface->Initialize())
    {
        delete notificationInterface;
        notificationInterface = NULL;
    }

    LogPrintf("returning notificationInterface\n");
    return notificationInterface;
}

void CZMQPublisherInterface::NotifyAPIStatus()
{
    for (std::list<CZMQAbstract*>::iterator i = notifiers.begin(); i!=notifiers.end(); )
    {
        CZMQAbstract *notifier = *i;
        if (notifier->NotifyAPIStatus())
        {
            i++;
        }
        else
        {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}

void CZMQPublisherInterface::NotifyMasternodeList()
{
    if(APIIsInWarmup())
        return;

    for (std::list<CZMQAbstract*>::iterator i = notifiers.begin(); i!=notifiers.end(); )
    {
        CZMQAbstract *notifier = *i;
        if (notifier->NotifyMasternodeList())
        {
            i++;
        }
        else
        {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}

void CZMQPublisherInterface::UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload)
{
    if(APIIsInWarmup())
        return;

    for (std::list<CZMQAbstract*>::iterator i = notifiers.begin(); i!=notifiers.end(); )
    {
        CZMQAbstract *notifier = *i;
        if (notifier->NotifyBlock(pindexNew))
        {
            i++;
        }
        else
        {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}

void CZMQPublisherInterface::WalletTransaction(const CTransaction& tx)
{
    if(APIIsInWarmup())
        return;

    for (std::list<CZMQAbstract*>::iterator i = notifiers.begin(); i!=notifiers.end(); )
    {
        CZMQAbstract *notifier = *i;
        if (notifier->NotifyTransaction(tx))
        {
            i++;
        }
        else
        {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}

void CZMQPublisherInterface::NotifyTransactionLock(const CTransaction& tx)
{
    if(APIIsInWarmup())
        return;

    for (std::list<CZMQAbstract*>::iterator i = notifiers.begin(); i!=notifiers.end(); )
    {
        CZMQAbstract *notifier = *i;
        if (notifier->NotifyTransactionLock(tx))
        {
            i++;
        }
        else
        {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}

void CZMQPublisherInterface::UpdatedMasternode(CDeterministicMNCPtr masternode)
{
    if(APIIsInWarmup())
        return;

    for (std::list<CZMQAbstract*>::iterator i = notifiers.begin(); i!=notifiers.end(); )
    {
        CZMQAbstract *notifier = *i;
        if (notifier->NotifyMasternodeUpdate(masternode))
        {
            i++;
        }
        else
        {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}

void CZMQPublisherInterface::UpdatedSettings(std::string update)
{
    if(APIIsInWarmup())
        return;

    for (std::list<CZMQAbstract*>::iterator i = notifiers.begin(); i!=notifiers.end(); )
    {
        CZMQAbstract *notifier = *i;
        if (notifier->NotifySettingsUpdate(update))
        {
            i++;
        }
        else
        {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}

void CZMQPublisherInterface::NotifyTxoutLock(COutPoint txout, bool isLocked) {
    if(APIIsInWarmup())
        return;

    for (std::list<CZMQAbstract*>::iterator i = notifiers.begin(); i!=notifiers.end(); )
    {
        CZMQAbstract *notifier = *i;
        if (notifier->NotifyTxoutLock(txout, isLocked))
        {
            i++;
        }
        else
        {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }

}