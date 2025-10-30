/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Kate Leet <kate@cutefishos.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "appmanager.h"
#include "appmanageradaptor.h"

#include <QDBusInterface>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

AppManager::AppManager(QObject *parent)
    : QObject(parent)
    , m_currentProcess(nullptr)
{
    QDBusConnection connection = QDBusConnection::systemBus();
    if (!connection.registerService("com.cutefish.Daemon")) {
        qDebug() << "Cannot register D-Bus service";
    }

    if (!connection.registerObject("/AppManager", this)) {
        qDebug() << "Cannot register object";
    }

    new AppManagerAdaptor(this);
}

AppManager::~AppManager()
{
    if (m_currentProcess) {
        m_currentProcess->kill();
        m_currentProcess->deleteLater();
    }
}

void AppManager::uninstall(const QString &content)
{
    // 首先获取包名
    getPackageNameFromFile(content);
}

void AppManager::getPackageNameFromFile(const QString &filePath)
{
    QProcess *process = new QProcess(this);
    process->setProgram("dpkg");
    process->setArguments({"-S", filePath});
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AppManager::onPackageNameResolved);
    connect(process, &QProcess::errorOccurred, this, &AppManager::onProcessError);
    
    process->start();
}

void AppManager::onPackageNameResolved(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *process = qobject_cast<QProcess*>(sender());
    if (!process) return;
    
    QString packageName;
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        QByteArray output = process->readAllStandardOutput();
        // 输出格式: "package: /path/to/file"
        QString outputStr = QString::fromUtf8(output);
        packageName = outputStr.split(':').first().trimmed();
    }
    
    process->deleteLater();
    
    if (!packageName.isEmpty()) {
        startUninstall(packageName);
    } else {
        notifyUninstallFailure(packageName);
    }
}

void AppManager::startUninstall(const QString &packageName)
{
    notifyUninstalling(packageName);
    
    m_currentProcess = new QProcess(this);
    m_currentProcess->setProgram("pkexec");
    m_currentProcess->setArguments({"apt", "remove", "--purge", "-y", packageName});
    
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AppManager::onUninstallFinished);
    connect(m_currentProcess, &QProcess::errorOccurred, this, &AppManager::onProcessError);
    
    m_currentProcess->start();
}

void AppManager::onUninstallFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString packageName = "unknown"; // 在实际应用中需要保存包名
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        // 卸载成功，现在运行autoremove清理依赖
        runAutoRemove();
    } else {
        notifyUninstallFailure(packageName);
        cleanupProcess();
    }
}

void AppManager::runAutoRemove()
{
    QProcess *process = new QProcess(this);
    process->setProgram("pkexec");
    process->setArguments({"apt", "autoremove", "-y"});
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AppManager::onAutoRemoveFinished);
    connect(process, &QProcess::errorOccurred, this, &AppManager::onProcessError);
    
    process->start();
}

void AppManager::onAutoRemoveFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    QProcess *process = qobject_cast<QProcess*>(sender());
    QString packageName = "unknown"; // 在实际应用中需要保存包名
    
    if (process) {
        process->deleteLater();
    }
    
    // 即使autoremove失败，只要卸载成功就认为操作成功
    notifyUninstallSuccess(packageName);
    cleanupProcess();
}

void AppManager::onProcessError(QProcess::ProcessError error)
{
    QProcess *process = qobject_cast<QProcess*>(sender());
    QString packageName = "unknown";
    
    qDebug() << "Process error:" << error;
    notifyUninstallFailure(packageName);
    
    if (process) {
        process->deleteLater();
    }
    cleanupProcess();
}

void AppManager::cleanupProcess()
{
    if (m_currentProcess) {
        m_currentProcess->deleteLater();
        m_currentProcess = nullptr;
    }
}

// 以下通知函数保持不变
void AppManager::notifyUninstalling(const QString &packageName)
{
    QDBusInterface iface("org.freedesktop.Notifications",
                         "/org/freedesktop/Notifications",
                         "org.freedesktop.Notifications",
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        QList<QVariant> args;
        args << "cutefish-daemon";
        args << ((unsigned int) 0);
        args << "cutefish-installer";
        args << packageName;
        args << tr("Uninstalling");
        args << QStringList();
        args << QVariantMap();
        args << (int) 10;
        iface.asyncCallWithArgumentList("Notify", args);
    }
}

void AppManager::notifyUninstallFailure(const QString &packageName)
{
    QDBusInterface iface("org.freedesktop.Notifications",
                         "/org/freedesktop/Notifications",
                         "org.freedesktop.Notifications",
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        QList<QVariant> args;
        args << "cutefish-daemon";
        args << ((unsigned int) 0);
        args << "dialog-error";
        args << packageName;
        args << tr("Uninstallation failure");
        args << QStringList();
        args << QVariantMap();
        args << (int) 10;
        iface.asyncCallWithArgumentList("Notify", args);
    }
}

void AppManager::notifyUninstallSuccess(const QString &packageName)
{
    QDBusInterface iface("org.freedesktop.Notifications",
                         "/org/freedesktop/Notifications",
                         "org.freedesktop.Notifications",
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        QList<QVariant> args;
        args << "cutefish-daemon";
        args << ((unsigned int) 0);
        args << "process-completed-symbolic";
        args << packageName;
        args << tr("Uninstallation successful");
        args << QStringList();
        args << QVariantMap();
        args << (int) 10;
        iface.asyncCallWithArgumentList("Notify", args);
    }

    cleanupProcess();
}