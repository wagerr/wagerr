// Copyright (c) 2018 The WAGERR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "governancepage.h"
#include "ui_governancepage.h"

#include "activemasternode.h"
#include "clientmodel.h"
#include "masternode-budget.h"
#include "masternode-sync.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "utilmoneystr.h"
#include "walletmodel.h"
#include "askpassphrasedialog.h"

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

void GovernancePage::SendVote(std::string strHash, int nVote)
{
    uint256 hash = uint256(strHash);
    int failed = 0, success = 0;
    std::string mnresult;
    for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
        std::string errorMessage;
        std::vector<unsigned char> vchMasterNodeSignature;
        std::string strMasterNodeSignMessage;

        CPubKey pubKeyCollateralAddress;
        CKey keyCollateralAddress;
        CPubKey pubKeyMasternode;
        CKey keyMasternode;
        
        if (!obfuScationSigner.SetKey(mne.getPrivKey(), errorMessage, keyMasternode, pubKeyMasternode)) {
            mnresult += mne.getAlias() + ": " + "Masternode signing error, could not set key correctly: " + errorMessage + "<br />";
            failed++;
            continue;
        }

        CMasternode* pmn = mnodeman.Find(pubKeyMasternode);
        if (pmn == NULL) {
            mnresult += mne.getAlias() + ": " + "Can't find masternode by pubkey" + "<br />";
            failed++;
            continue;
        }

        CBudgetVote vote(pmn->vin, hash, nVote);
        if (!vote.Sign(keyMasternode, pubKeyMasternode)) {
            mnresult += mne.getAlias() + ": " + "Failure to sign" + "<br />";
            failed++;
            continue;
        }

        std::string strError = "";
        if (budget.UpdateProposal(vote, NULL, strError)) {
            budget.mapSeenMasternodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
            vote.Relay();
            mnresult += mne.getAlias() + ": " + "Success!" + "<br />";
            success++;
        } else {
            mnresult += mne.getAlias() + ": " + "Failed!" + "<br />";
            failed++;
        }

    }
    
    if (success > 0) updateProposalList(true);

    QString strMessage = QString("Successfully voted with %1 masternode(s), failed with %2").arg(success).arg(failed);
    strMessage += "<br /><br /><hr />";
    strMessage += QString::fromStdString(mnresult);
    QMessageBox::information(this, tr("Vote Results"), strMessage, QMessageBox::Ok, QMessageBox::Ok);
    
}

void GovernancePage::updateProposalList(bool fForce)
{
    int nCountMyMasternodes = masternodeConfig.getCount();
    std::vector<CBudgetProposal*> winningProps = budget.GetAllProposals();
    int nGridRow = 0, nGridCol = 0;
    for (CBudgetProposal* pbudgetProposal : winningProps) {
        if (!pbudgetProposal->fValid) continue;
        if (pbudgetProposal->GetRemainingPaymentCount() < 1) continue;
        QFrame* proposalFrame = new QFrame();
        proposalFrame->setObjectName(QStringLiteral("proposalFrame"));
        proposalFrame->setFrameShape(QFrame::StyledPanel);
        
        QVBoxLayout* proposalItem = new QVBoxLayout(proposalFrame);
        proposalItem->setSpacing(0);
        proposalItem->setObjectName(QStringLiteral("proposalItem"));
        
        QHBoxLayout* proposalInfo = new QHBoxLayout();
        proposalInfo->setSpacing(0);
        proposalInfo->setObjectName(QStringLiteral("proposalInfo"));

        QLabel* strProposalHash = new QLabel();
        strProposalHash->setObjectName(QStringLiteral("strProposalHash"));
        strProposalHash->setText(QString::fromStdString(pbudgetProposal->GetHash().ToString()));
        strProposalHash->setVisible(false);
        QLabel* strProposalName = new QLabel();
        strProposalName->setObjectName(QStringLiteral("strProposalName"));
        strProposalName->setText(QString::fromStdString(pbudgetProposal->GetName()));
        QLabel* strMonthlyPayout = new QLabel();
        strMonthlyPayout->setObjectName(QStringLiteral("strMonthlyPayout"));
        strMonthlyPayout->setText(QString::fromStdString(FormatMoney(pbudgetProposal->GetAmount())));
        strMonthlyPayout->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        proposalInfo->addWidget(strProposalName);
        proposalInfo->addWidget(strProposalHash);
        proposalInfo->addStretch();
        proposalInfo->addWidget(strMonthlyPayout);
        proposalItem->addLayout(proposalInfo);
        
        QLabel* strProposalURL = new QLabel();
        strProposalURL->setObjectName(QStringLiteral("strProposalURL"));
        QString strURL = QString::fromStdString(pbudgetProposal->GetURL());
        strProposalURL->setText("<a href=\"" + strURL + "\">" + strURL + "</a>");
        strProposalURL->setTextFormat(Qt::RichText);
        strProposalURL->setTextInteractionFlags(Qt::TextBrowserInteraction);
        strProposalURL->setOpenExternalLinks(true);
        proposalItem->addWidget(strProposalURL);
        
        QHBoxLayout* proposalVotes = new QHBoxLayout();
        
        QToolButton* yesButton = new QToolButton();
        yesButton->setIcon(QIcon(":/icons/yesvote"));
        if (nCountMyMasternodes < 0)
            yesButton->setEnabled(false);
        connect(yesButton, &QPushButton::clicked, this, [this]{ voteButton_clicked(1); });
        QLabel* labelYesVotes = new QLabel();
        labelYesVotes->setText(tr("Yes:"));
        QLabel* yesVotes = new QLabel();
        yesVotes->setText(QString::number(pbudgetProposal->GetYeas()));

        QToolButton* noButton = new QToolButton();
        noButton->setIcon(QIcon(":/icons/novote"));
        if (nCountMyMasternodes < 0)
            noButton->setEnabled(false);
        connect(noButton, &QPushButton::clicked, this, [this]{ voteButton_clicked(2); });
        QLabel* labelNoVotes = new QLabel();
        labelNoVotes->setText(tr("No:"));
        QLabel* noVotes = new QLabel();
        noVotes->setText(QString::number(pbudgetProposal->GetNays()));
        
        proposalVotes->addWidget(yesButton);
        proposalVotes->addWidget(labelYesVotes);
        proposalVotes->addWidget(yesVotes);
        proposalVotes->addStretch();
        proposalVotes->addWidget(labelNoVotes);
        proposalVotes->addWidget(noVotes);
        proposalVotes->addWidget(noButton);
        proposalItem->addLayout(proposalVotes);

        proposalFrame->setMaximumHeight(150);
        ui->proposalGrid->addWidget(proposalFrame, nGridRow, nGridCol);
        
        if (nGridCol == 1 ) {
            ++nGridRow;
            nGridCol = -1;
        }
        ++nGridCol;
    }
}

void GovernancePage::voteButton_clicked(int nVote)
{
    if (!walletModel) return;

    // Request unlock if wallet was locked or unlocked for mixing:
    WalletModel::EncryptionStatus encStatus = walletModel->getEncryptionStatus();
    if (encStatus == walletModel->Locked) {
        WalletModel::UnlockContext ctx(walletModel->requestUnlock(AskPassphraseDialog::Context::UI_Vote, true));
        if (!ctx.isValid()) {
            // Unlock wallet was cancelled
            QMessageBox::warning(this, tr("Wallet Locked"), tr("You must unlock your wallet to vote."), QMessageBox::Ok, QMessageBox::Ok);
            return;
        }
    }
    
    // Find out which frame's button was clicked
    QWidget *buttonWidget = qobject_cast<QWidget*>(sender());
    if(!buttonWidget)return;
    QFrame* frame = qobject_cast<QFrame*>(buttonWidget->parentWidget());
    if(!frame)return;

    // Extract the values we want
    QLabel *strProposalName = frame->findChild<QLabel *>("strProposalName");
    QLabel *strProposalHash = frame->findChild<QLabel *>("strProposalHash");
    QLabel *strProposalURL = frame->findChild<QLabel *>("strProposalURL");
    
    // Display message box
    QString questionString = tr("Do you want to vote %1 on").arg(nVote == 1 ? "yes" : "no") + " " + strProposalName->text() + " ";
    questionString += tr("using all your masternodes?");
    questionString += "<br /><br />";
    questionString += tr("Proposal Hash:") + " " + strProposalHash->text() + "<br />";
    questionString += tr("Proposal URL:") + " " + strProposalURL->text();
    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm Vote"),
        questionString,
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) {
        return;
    }
    
    SendVote(strProposalHash->text().toStdString(), nVote);
}

void GovernancePage::on_UpdateButton_clicked()
{
    updateProposalList(true);
}
