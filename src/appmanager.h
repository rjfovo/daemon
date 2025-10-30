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

#ifndef APPMANAGER_H
#define APPMANAGER_H

#include <QObject>
#include <QProcess>

class AppManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.AppManager")

public:
    explicit AppManager(QObject *parent = nullptr);
    ~AppManager();

public Q_SLOTS:
    /**
     * @brief 卸载应用程序
     * @param content 应用程序文件路径或包名
     */
    Q_SCRIPTABLE void uninstall(const QString &content);

private Q_SLOTS:
    /**
     * @brief 解析包名完成后的处理
     */
    void onPackageNameResolved(int exitCode, QProcess::ExitStatus exitStatus);
    
    /**
     * @brief 卸载过程完成后的处理
     */
    void onUninstallFinished(int exitCode, QProcess::ExitStatus exitStatus);
    
    /**
     * @brief 自动清理依赖完成后的处理
     */
    void onAutoRemoveFinished(int exitCode, QProcess::ExitStatus exitStatus);
    
    /**
     * @brief 处理进程错误
     */
    void onProcessError(QProcess::ProcessError error);

private:
    /**
     * @brief 根据文件路径获取包名
     * @param filePath 应用程序文件路径
     */
    void getPackageNameFromFile(const QString &filePath);
    
    /**
     * @brief 开始卸载包
     * @param packageName 包名
     */
    void startUninstall(const QString &packageName);
    
    /**
     * @brief 运行自动清理未使用的依赖
     */
    void runAutoRemove();
    
    /**
     * @brief 清理当前进程
     */
    void cleanupProcess();
    
    /**
     * @brief 发送正在卸载的通知
     * @param packageName 包名
     */
    void notifyUninstalling(const QString &packageName);
    
    /**
     * @brief 发送卸载失败的通知
     * @param packageName 包名
     */
    void notifyUninstallFailure(const QString &packageName);
    
    /**
     * @brief 发送卸载成功的通知
     * @param packageName 包名
     */
    void notifyUninstallSuccess(const QString &packageName);

private:
    QProcess *m_currentProcess;  // 当前正在运行的进程
};

#endif // APPMANAGER_H