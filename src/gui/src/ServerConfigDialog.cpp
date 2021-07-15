/*
 * synergy -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
 * 
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 * 
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScreenSettingsDialog.h"
#include "ServerConfigDialog.h"
#include "ServerConfig.h"
#include "HotkeyDialog.h"
#include "ActionDialog.h"

#include <QtCore>
#include <QtGui>
#include <QMessageBox>
#include <QFileDialog>

ServerConfigDialog::ServerConfigDialog(QWidget* parent, ServerConfig& config) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint),
    Ui::ServerConfigDialogBase(),
    m_OrigServerConfig(config),
    m_ServerConfig(config),
    m_ScreenSetupModel(serverConfig().screens(), serverConfig().numColumns(), serverConfig().numRows()),
    m_Message("")
{
    setupUi(this);

    m_pEditConfigFile->setText(serverConfig().getConfigFile());
    m_pCheckBoxUseExternalConfig->setChecked(serverConfig().getUseExternalConfig());
    m_pCheckBoxHeartbeat->setChecked(serverConfig().hasHeartbeat());
    m_pSpinBoxHeartbeat->setValue(serverConfig().heartbeat());

    m_pCheckBoxRelativeMouseMoves->setChecked(serverConfig().relativeMouseMoves());
    m_pCheckBoxWin32KeepForeground->setChecked(serverConfig().win32KeepForeground());

    m_pCheckBoxSwitchDelay->setChecked(serverConfig().hasSwitchDelay());
    m_pSpinBoxSwitchDelay->setValue(serverConfig().switchDelay());

    m_pCheckBoxSwitchDoubleTap->setChecked(serverConfig().hasSwitchDoubleTap());
    m_pSpinBoxSwitchDoubleTap->setValue(serverConfig().switchDoubleTap());

    m_pCheckBoxCornerTopLeft->setChecked(serverConfig().switchCorner(BaseConfig::TopLeft));
    m_pCheckBoxCornerTopRight->setChecked(serverConfig().switchCorner(BaseConfig::TopRight));
    m_pCheckBoxCornerBottomLeft->setChecked(serverConfig().switchCorner(BaseConfig::BottomLeft));
    m_pCheckBoxCornerBottomRight->setChecked(serverConfig().switchCorner(BaseConfig::BottomRight));
    m_pSpinBoxSwitchCornerSize->setValue(serverConfig().switchCornerSize());
    m_pCheckBoxDisableLockToScreen->setChecked(serverConfig().disableLockToScreen());

    m_pCheckBoxIgnoreAutoConfigClient->setChecked(serverConfig().ignoreAutoConfigClient());

    m_pCheckBoxEnableClipboard->setChecked(serverConfig().clipboardSharing());
	int clipboardSharingSizeM = static_cast<int>(serverConfig().clipboardSharingSize() / 1024);
    m_pSpinBoxClipboardSizeLimit->setValue(clipboardSharingSizeM);
	m_pSpinBoxClipboardSizeLimit->setEnabled(serverConfig().clipboardSharing());

    foreach(const Hotkey& hotkey, serverConfig().hotkeys())
        m_pListHotkeys->addItem(hotkey.text());

    m_pScreenSetupView->setModel(&m_ScreenSetupModel);

    auto& screens = serverConfig().screens();
    auto server = std::find_if(screens.begin(), screens.end(), [this](const Screen& screen){ return (screen.name() == serverConfig().getServerName());});

    if (server == screens.end()) {
        Screen serverScreen(serverConfig().getServerName());
        serverScreen.markAsServer();
        model().screen(serverConfig().numColumns() / 2, serverConfig().numRows() / 2) = serverScreen;
    }
    else {
        server->markAsServer();
    }

    m_pButtonAddComputer->setEnabled(!model().isFull());
    connect(m_pTrashScreenWidget, SIGNAL(screenRemoved()), this, SLOT(onScreenRemoved()));
}

void ServerConfigDialog::showEvent(QShowEvent* event)
{
    QDialog::show();

    if (!m_Message.isEmpty())
    {
        // TODO: ideally this massage box should pop up after the dialog is shown
        QMessageBox::information(this, tr("Configure server"), m_Message);
    }
}

void ServerConfigDialog::accept()
{
    if (m_pCheckBoxUseExternalConfig->isChecked() &&
        !QFile::exists(m_pEditConfigFile->text()))
    {
        auto title = tr("Configuration filename invalid");
        auto description = tr("You have not filled in a valid configuration file for the synergy server. "
                              "Do you want to browse for the configuration file now?");

        auto selectedButton = QMessageBox::warning(this, title, description, QMessageBox::Yes | QMessageBox::No);

        if (selectedButton != QMessageBox::Yes ||
            !on_m_pButtonBrowseConfigFile_clicked())
        {
           return;
        }
    }

    serverConfig().setConfigFile(m_pEditConfigFile->text());
    serverConfig().setUseExternalConfig(m_pCheckBoxUseExternalConfig->isChecked());

    serverConfig().haveHeartbeat(m_pCheckBoxHeartbeat->isChecked());
    serverConfig().setHeartbeat(m_pSpinBoxHeartbeat->value());

    serverConfig().setRelativeMouseMoves(m_pCheckBoxRelativeMouseMoves->isChecked());
    serverConfig().setWin32KeepForeground(m_pCheckBoxWin32KeepForeground->isChecked());

    serverConfig().haveSwitchDelay(m_pCheckBoxSwitchDelay->isChecked());
    serverConfig().setSwitchDelay(m_pSpinBoxSwitchDelay->value());

    serverConfig().haveSwitchDoubleTap(m_pCheckBoxSwitchDoubleTap->isChecked());
    serverConfig().setSwitchDoubleTap(m_pSpinBoxSwitchDoubleTap->value());

    serverConfig().setSwitchCorner(BaseConfig::TopLeft, m_pCheckBoxCornerTopLeft->isChecked());
    serverConfig().setSwitchCorner(BaseConfig::TopRight, m_pCheckBoxCornerTopRight->isChecked());
    serverConfig().setSwitchCorner(BaseConfig::BottomLeft, m_pCheckBoxCornerBottomLeft->isChecked());
    serverConfig().setSwitchCorner(BaseConfig::BottomRight, m_pCheckBoxCornerBottomRight->isChecked());
    serverConfig().setSwitchCornerSize(m_pSpinBoxSwitchCornerSize->value());
    serverConfig().setIgnoreAutoConfigClient(m_pCheckBoxIgnoreAutoConfigClient->isChecked());
    serverConfig().setDisableLockToScreen(m_pCheckBoxDisableLockToScreen->isChecked());
    serverConfig().setClipboardSharingSize(m_pSpinBoxClipboardSizeLimit->value() * 1024);
    serverConfig().setClipboardSharing(m_pCheckBoxEnableClipboard->isChecked()
               && m_pSpinBoxClipboardSizeLimit->value());

    // now that the dialog has been accepted, copy the new server config to the original one,
    // which is a reference to the one in MainWindow.
    setOrigServerConfig(serverConfig());

    QDialog::accept();
}

void ServerConfigDialog::on_m_pButtonNewHotkey_clicked()
{
    Hotkey hotkey;
    HotkeyDialog dlg(this, hotkey);
    if (dlg.exec() == QDialog::Accepted)
    {
        serverConfig().hotkeys().append(hotkey);
        m_pListHotkeys->addItem(hotkey.text());
    }
}

void ServerConfigDialog::on_m_pButtonEditHotkey_clicked()
{
    int idx = m_pListHotkeys->currentRow();
    Q_ASSERT(idx >= 0 && idx < serverConfig().hotkeys().size());
    Hotkey& hotkey = serverConfig().hotkeys()[idx];
    HotkeyDialog dlg(this, hotkey);
    if (dlg.exec() == QDialog::Accepted)
        m_pListHotkeys->currentItem()->setText(hotkey.text());
}

void ServerConfigDialog::on_m_pButtonRemoveHotkey_clicked()
{
    int idx = m_pListHotkeys->currentRow();
    Q_ASSERT(idx >= 0 && idx < serverConfig().hotkeys().size());
    serverConfig().hotkeys().removeAt(idx);
    m_pListActions->clear();
    delete m_pListHotkeys->item(idx);
}

void ServerConfigDialog::on_m_pListHotkeys_itemSelectionChanged()
{
    bool itemsSelected = !m_pListHotkeys->selectedItems().isEmpty();
    m_pButtonEditHotkey->setEnabled(itemsSelected);
    m_pButtonRemoveHotkey->setEnabled(itemsSelected);
    m_pButtonNewAction->setEnabled(itemsSelected);

    if (itemsSelected && serverConfig().hotkeys().size() > 0)
    {
        m_pListActions->clear();

        int idx = m_pListHotkeys->row(m_pListHotkeys->selectedItems()[0]);

        // There's a bug somewhere around here: We get idx == 1 right after we deleted the next to last item, so idx can
        // only possibly be 0. GDB shows we got called indirectly from the delete line in
        // on_m_pButtonRemoveHotkey_clicked() above, but the delete is of course necessary and seems correct.
        // The while() is a generalized workaround for all that and shouldn't be required.
        while (idx >= 0 && idx >= serverConfig().hotkeys().size())
            idx--;

        Q_ASSERT(idx >= 0 && idx < serverConfig().hotkeys().size());

        const Hotkey& hotkey = serverConfig().hotkeys()[idx];
        foreach(const Action& action, hotkey.actions())
            m_pListActions->addItem(action.text());
    }
}

void ServerConfigDialog::on_m_pButtonNewAction_clicked()
{
    int idx = m_pListHotkeys->currentRow();
    Q_ASSERT(idx >= 0 && idx < serverConfig().hotkeys().size());
    Hotkey& hotkey = serverConfig().hotkeys()[idx];

    Action action;
    ActionDialog dlg(this, serverConfig(), hotkey, action);
    if (dlg.exec() == QDialog::Accepted)
    {
        hotkey.actions().append(action);
        m_pListActions->addItem(action.text());
    }
}

void ServerConfigDialog::on_m_pButtonEditAction_clicked()
{
    int idxHotkey = m_pListHotkeys->currentRow();
    Q_ASSERT(idxHotkey >= 0 && idxHotkey < serverConfig().hotkeys().size());
    Hotkey& hotkey = serverConfig().hotkeys()[idxHotkey];

    int idxAction = m_pListActions->currentRow();
    Q_ASSERT(idxAction >= 0 && idxAction < hotkey.actions().size());
    Action& action = hotkey.actions()[idxAction];

    ActionDialog dlg(this, serverConfig(), hotkey, action);
    if (dlg.exec() == QDialog::Accepted)
        m_pListActions->currentItem()->setText(action.text());
}

void ServerConfigDialog::on_m_pButtonRemoveAction_clicked()
{
    int idxHotkey = m_pListHotkeys->currentRow();
    Q_ASSERT(idxHotkey >= 0 && idxHotkey < serverConfig().hotkeys().size());
    Hotkey& hotkey = serverConfig().hotkeys()[idxHotkey];

    int idxAction = m_pListActions->currentRow();
    Q_ASSERT(idxAction >= 0 && idxAction < hotkey.actions().size());

    hotkey.actions().removeAt(idxAction);
    delete m_pListActions->currentItem();
}

void ServerConfigDialog::on_m_pCheckBoxEnableClipboard_stateChanged(int const state)
{
    m_pSpinBoxClipboardSizeLimit->setEnabled (state == Qt::Checked);
    if ((state == Qt::Checked) && (!m_pSpinBoxClipboardSizeLimit->value())) {
        int size = static_cast<int>((serverConfig().defaultClipboardSharingSize() + 512) / 1024);
        m_pSpinBoxClipboardSizeLimit->setValue (size ? size : 1);
    }
}

void ServerConfigDialog::on_m_pListActions_itemSelectionChanged()
{
    m_pButtonEditAction->setEnabled(!m_pListActions->selectedItems().isEmpty());
    m_pButtonRemoveAction->setEnabled(!m_pListActions->selectedItems().isEmpty());
}

void ServerConfigDialog::on_m_pButtonAddComputer_clicked()
{
    Screen newScreen("");

    ScreenSettingsDialog dlg(this, &newScreen, &model().m_Screens);
    if (dlg.exec() == QDialog::Accepted)
    {
        model().addScreen(newScreen);
    }

    m_pButtonAddComputer->setEnabled(!model().isFull());
}

void ServerConfigDialog::onScreenRemoved()
{
    m_pButtonAddComputer->setEnabled(true);
}

void ServerConfigDialog::on_m_pCheckBoxUseExternalConfig_toggled(bool checked)
{
    m_pLabelConfigFile->setEnabled(checked);
    m_pEditConfigFile->setEnabled(checked);
    m_pButtonBrowseConfigFile->setEnabled(checked);

    m_pTabWidget->setTabEnabled(0, !checked);
    m_pTabWidget->setTabEnabled(1, !checked);
    m_pTabWidget->setTabEnabled(2, !checked);
}

bool ServerConfigDialog::on_m_pButtonBrowseConfigFile_clicked()
{
#if defined(Q_OS_WIN)
    const QString synergyConfigFilter(QObject::tr("Synergy Configurations (*.sgc);;All files (*.*)"));
#else
    const QString synergyConfigFilter(QObject::tr("Synergy Configurations (*.conf);;All files (*.*)"));
#endif

    QString fileName = QFileDialog::getOpenFileName(this, tr("Browse for a synergys config file"), QString(), synergyConfigFilter);

    if (!fileName.isEmpty())
    {
        m_pEditConfigFile->setText(fileName);
        return true;
    }

    return false;
}
