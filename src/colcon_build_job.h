// Build job
// Author: Max Schwarz <max.schwarz@online.de>

#ifndef COLCON_BUILD_JOB_H
#define COLCON_BUILD_JOB_H

#include <outputview/outputexecutejob.h>

#include <QProcess>

namespace KDevelop
{
    class IProject;
    class CommandExecutor;
}

class ColconBuildJob : public KDevelop::OutputExecuteJob
{
Q_OBJECT
public:
    enum ErrorType {
        Failed
    };

    explicit ColconBuildJob(KDevelop::IProject* project, QObject* parent = nullptr);

protected Q_SLOTS:
    void postProcessStdout(const QStringList& lines) override;
    void postProcessStderr(const QStringList& lines) override;

private:
    void appendLines(const QStringList& lines);
};

#endif
