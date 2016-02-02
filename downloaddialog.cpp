/*

Copyright 2016 Adam Reichold

This file is part of QMediathekView.

QMediathekView is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

QMediathekView is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with QMediathekView.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "downloaddialog.h"

#include <QAction>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>

#include "settings.h"
#include "model.h"

namespace QMediathekView
{

DownloadDialog::DownloadDialog(
    const Settings& settings,
    const Model& model,
    const QModelIndex& index,
    QNetworkAccessManager* networkManager,
    QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_url(model.url(index))
    , m_urlLarge(model.urlLarge(index))
    , m_urlSmall(model.urlSmall(index))
    , m_networkManager(networkManager)
    , m_networkReply(nullptr)
    , m_file(nullptr)
{
    setWindowTitle(tr("Download '%1'").arg(model.title(index)));

    const auto layout = new QGridLayout(this);
    setLayout(layout);

    layout->setRowStretch(2, 1);
    layout->setColumnStretch(1, 1);

    const auto filePathLabel = new QLabel(tr("File"), this);
    layout->addWidget(filePathLabel, 0, 0);

    m_filePathEdit = new QLineEdit(this);
    layout->addWidget(m_filePathEdit, 0, 1, 1, 4);

    const auto selectFilePathAction = m_filePathEdit->addAction(QIcon::fromTheme(QStringLiteral("document-open")), QLineEdit::TrailingPosition);
    connect(selectFilePathAction, &QAction::triggered, this, &DownloadDialog::selectFilePath);

    m_defaultButton = new QRadioButton(tr("Default"), this);
    layout->addWidget(m_defaultButton, 1, 2);

    m_smallButton = new QRadioButton(tr("Small"), this);
    layout->addWidget(m_smallButton, 1, 3);

    m_largeButton = new QRadioButton(tr("Large"), this);
    layout->addWidget(m_largeButton, 1, 4);

    const auto buttonsWidget = new QWidget(this);
    layout->addWidget(buttonsWidget, 3, 2, 1, 3);
    layout->setAlignment(buttonsWidget, Qt::AlignRight);

    const auto buttonsLayout = new QBoxLayout(QBoxLayout::LeftToRight, buttonsWidget);
    buttonsWidget->setLayout(buttonsLayout);
    buttonsLayout->setSizeConstraint(QLayout::SetFixedSize);

    m_startButton = new QPushButton(QIcon::fromTheme(QStringLiteral("call-start")), QString(), buttonsWidget);
    buttonsLayout->addWidget(m_startButton);

    m_cancelButton = new QPushButton(QIcon::fromTheme(QStringLiteral("call-stop")), QString(), buttonsWidget);
    buttonsLayout->addWidget(m_cancelButton);

    connect(m_startButton, &QPushButton::pressed, this, &DownloadDialog::start);
    connect(m_cancelButton, &QPushButton::pressed, this, &DownloadDialog::cancel);

    m_progressBar = new QProgressBar(this);
    layout->addWidget(m_progressBar, 4, 0, 1, 5);

    m_defaultButton->setDisabled(m_url.isEmpty());
    m_smallButton->setDisabled(m_urlSmall.isEmpty());
    m_largeButton->setDisabled(m_urlLarge.isEmpty());

    auto firstButton = m_defaultButton;
    auto secondButton = m_smallButton;
    auto thirdButton = m_largeButton;

    switch (m_settings.preferredUrl())
    {
    default:
    case Url::Default:
        break;
    case Url::Small:
        firstButton = m_smallButton;
        secondButton = m_defaultButton;
        thirdButton = m_largeButton;
        break;
    case Url::Large:
        firstButton = m_largeButton;
        secondButton = m_defaultButton;
        thirdButton = m_smallButton;
        break;
    }

    if (firstButton->isEnabled())
    {
        firstButton->setChecked(true);
    }
    else if (secondButton->isEnabled())
    {
        secondButton->setChecked(true);
    }
    else if (thirdButton->isEnabled())
    {
        thirdButton->setChecked(true);
    }
    else
    {
        m_startButton->setEnabled(false);
        m_filePathEdit->setEnabled(false);
    }

    m_cancelButton->setEnabled(false);

    m_filePathEdit->setText(m_settings.downloadFolder().absoluteFilePath(selectedUrl().fileName()));
}

DownloadDialog::~DownloadDialog()
{
    if (m_networkReply)
    {
        delete m_networkReply;
        m_networkReply = nullptr;
    }
}

void DownloadDialog::selectFilePath()
{
    const auto filePath = QFileDialog::getSaveFileName(
                              this, tr("Select file path"),
                              m_filePathEdit->text());

    if (!filePath.isNull())
    {
        m_filePathEdit->setText(filePath);
    }
}

void DownloadDialog::start()
{
    m_file = new QFile(m_filePathEdit->text());
    if (!m_file->open(QIODevice::WriteOnly))
    {
        delete m_file;
        m_file = nullptr;

        QMessageBox::critical(this, tr("Critical"), tr("Failed to open file for writing."));

        return;
    }

    QNetworkRequest request(selectedUrl());
    request.setHeader(QNetworkRequest::UserAgentHeader, m_settings.userAgent());

    m_networkReply = m_networkManager->get(request);

    connect(m_networkReply, &QNetworkReply::readyRead, this, &DownloadDialog::readyRead);
    connect(m_networkReply, &QNetworkReply::finished, this, &DownloadDialog::finished);

    connect(m_networkReply, &QNetworkReply::downloadProgress, this, &DownloadDialog::downloadProgress);

    m_startButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_filePathEdit->setEnabled(false);
}

void DownloadDialog::cancel()
{
    m_networkReply->abort();
}

void DownloadDialog::readyRead()
{
    if (m_networkReply->error())
    {
        return;
    }

    m_file->write(m_networkReply->readAll());
}

void DownloadDialog::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    m_progressBar->setValue(bytesReceived);
    m_progressBar->setMaximum(bytesTotal);
}

void DownloadDialog::finished()
{
    const auto reply = m_networkReply;

    m_networkReply->deleteLater();
    m_networkReply = nullptr;

    if (reply->error())
    {
        m_file->close();
        m_file->remove();

        delete m_file;
        m_file = nullptr;

        m_startButton->setEnabled(true);
        m_cancelButton->setEnabled(false);
        m_filePathEdit->setEnabled(true);

        return;
    }

    m_file->write(reply->readAll());
    m_file->close();

    delete m_file;
    m_file = nullptr;

    m_startButton->setEnabled(false);
    m_cancelButton->setEnabled(false);
    m_filePathEdit->setEnabled(false);
}

QUrl DownloadDialog::selectedUrl() const
{
    if (m_smallButton->isChecked())
    {
        return m_urlSmall;
    }

    if (m_largeButton->isChecked())
    {
        return m_urlLarge;
    }

    return m_url;
}

} // QMediathekView
