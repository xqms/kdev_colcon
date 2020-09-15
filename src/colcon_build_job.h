// Build job
// Author: Max Schwarz <max.schwarz@online.de>

#ifndef COLCON_BUILD_JOB_H
#define COLCON_BUILD_JOB_H

#include <outputview/outputjob.h>

#include <QProcess>

namespace KDevelop
{
    class IProject;
    class CommandExecutor;
}

class ColconBuildJob : public KDevelop::OutputJob
{
Q_OBJECT
public:
    enum ErrorType {
        FailedToStart = UserDefinedError,
        UnknownExecError,
        Crashed,
    };

    explicit ColconBuildJob(KDevelop::IProject* project, QObject* parent = nullptr);

    void start() override;
    bool doKill() override;

private Q_SLOTS:
    void procFinished(int exitcode);
    void procError(QProcess::ProcessError error);

private:
    QString m_workspace;
    KDevelop::CommandExecutor* m_exec = nullptr;
    bool m_killed = false;
};

#endif
