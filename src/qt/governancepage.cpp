// Copyright (c) 2018 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "governancepage.h"
#include "ui_governancepage.h"

#include "activemasternode.h"
#include "chainparams.h"
#include "clientmodel.h"
#include "masternode-budget.h"
#include "masternode-sync.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "utilmoneystr.h"
#include "walletmodel.h"
#include "askpassphrasedialog.h"
#include "proposalframe.h"

#include <QMessageBox>
#include <QString>
#include <QTimer>
#include <QToolButton>

GovernancePage::GovernancePage(QWidget* parent) : QWidget(parent),
                                                  ui(new Ui::GovernancePage),
                                                  clientModel(0),
                                                  walletModel(0)
{
    ui->setupUi(this);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateProposalList()));
    timer->start(100000);
    fLockUpdating = false;
}


GovernancePage::~GovernancePage()
{
    delete ui;
}

void GovernancePage::setClientModel(ClientModel* model)
{
    this->clientModel = model;
}

void GovernancePage::setWalletModel(WalletModel* model)
{
    this->walletModel = model;
}

void GovernancePage::lockUpdating(bool lock)
{
    fLockUpdating = lock;
}

struct sortProposalsByVotes
{
    bool operator() (const CBudgetProposal* left, const CBudgetProposal* right)
    {
        if (left != right)
            return (left->GetYeas() - left->GetNays() > right->GetYeas() - right->GetNays());
        return (left->nFeeTXHash > right->nFeeTXHash);
    }
};

void GovernancePage::updateProposalList()
{
    if (fLockUpdating) return;

    QLayoutItem* child;
    while ((child = ui->proposalGrid->takeAt(0)) != 0) {
        if (child->widget() != 0)
        {
            delete child->widget();
        }
        delete child;
    }

    std::vector<CBudgetProposal*> allotedProposals = budget.GetBudget();
    CAmount nTotalAllotted = 0;
    std::vector<CBudgetProposal*> proposalsList = budget.GetAllProposals();
    std::sort (proposalsList.begin(), proposalsList.end(), sortProposalsByVotes());
    int nRow = 0;
    CBlockIndex* pindexPrev;
    {
        LOCK(cs_main);
        pindexPrev = chainActive.Tip();
    }
    if (!pindexPrev) return;
    int nBlockStart = pindexPrev->nHeight - pindexPrev->nHeight % Params().GetBudgetCycleBlocks() + Params().GetBudgetCycleBlocks();
    int nBlocksLeft = nBlockStart - pindexPrev->nHeight;
    int nBlockEnd = nBlockStart + Params().GetBudgetCycleBlocks() - 1;
    int mnCount = mnodeman.CountEnabled(ActiveProtocol());

    for (CBudgetProposal* pbudgetProposal : proposalsList) {
        if (!pbudgetProposal->fValid) continue;
        if (pbudgetProposal->GetRemainingPaymentCount() < 1) continue;

        ProposalFrame* proposalFrame = new ProposalFrame();
        proposalFrame->setWalletModel(walletModel);
        proposalFrame->setProposal(pbudgetProposal);
        proposalFrame->setGovernancePage(this);

        if (std::find(allotedProposals.begin(), allotedProposals.end(), pbudgetProposal) != allotedProposals.end()) {
            nTotalAllotted += pbudgetProposal->GetAllotted();
            proposalFrame->setObjectName(QStringLiteral("proposalFramePassing"));
        } else if (!pbudgetProposal->IsEstablished()) {
            proposalFrame->setObjectName(QStringLiteral("proposalFrameNotEstablished"));
        } else if (pbudgetProposal->IsPassing(pindexPrev, nBlockStart, nBlockEnd, mnCount)) {
            proposalFrame->setObjectName(QStringLiteral("proposalFramePassingUnfunded"));
        } else {
            proposalFrame->setObjectName(QStringLiteral("proposalFrame"));
        }
        proposalFrame->setFrameShape(QFrame::StyledPanel);

        if (extendedProposal == pbudgetProposal)
            proposalFrame->extend();
        proposalFrame->setMaximumHeight(150);
        ui->proposalGrid->addWidget(proposalFrame, nRow);

        ++nRow;
    }

    ui->next_superblock_value->setText(QString::number(nBlockStart));
    ui->blocks_before_super_value->setText(QString::number(nBlocksLeft));
    ui->time_before_super_value->setText(QString::number(nBlocksLeft/60/24));
    ui->alloted_budget_value->setText(QString::number(nTotalAllotted/COIN));
    ui->unallocated_budget_value->setText(QString::number((budget.GetTotalBudget(pindexPrev->nHeight) - nTotalAllotted)/COIN));
    ui->masternode_count_value->setText(QString::number(mnodeman.CountEnabled(ActiveProtocol())));
}

void GovernancePage::setExtendedProposal(CBudgetProposal* proposal)
{
    bool update = false;
    if (extendedProposal != proposal)
        update = true;
    extendedProposal = proposal;
    if (update)
        updateProposalList();
}

void GovernancePage::on_UpdateButton_clicked()
{
    updateProposalList();
}
