// Build job
// Author: Max Schwarz <max.schwarz@online.de>

#include "colcon_build_job.h"

#include <KLocalizedString>

#include <interfaces/iproject.h>
#include <outputview/outputdelegate.h>
#include <outputview/outputmodel.h>
#include <util/commandexecutor.h>
#include <util/environmentprofilelist.h>
#include <util/path.h>

ColconBuildJob::ColconBuildJob(KDevelop::IProject* project, QObject* parent)
 : OutputJob{parent}
{
    setCapabilities(Killable);

    QString title = i18nc("Building: <project name>", "Building: %1", project->name());
    setTitle(title);
    setObjectName(title);
    setDelegate(new KDevelop::OutputDelegate);

    m_workspace = project->path().parent().toLocalFile();
}

void ColconBuildJob::start()
{
    setStandardToolView(KDevelop::IOutputView::BuildView);
    setBehaviours(KDevelop::IOutputView::AllowUserClose | KDevelop::IOutputView::AutoScroll);

    auto* model = new KDevelop::OutputModel({});
    model->setFilteringStrategy(KDevelop::OutputModel::CompilerFilter);
    setModel(model);

    startOutput();

    QString cmd = ". /opt/ros/melodic/setup.sh && colcon build"
        " --symlink-install"
        " --event-handlers status+ console_start_end-"
        " --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo";
    m_exec = new KDevelop::CommandExecutor(cmd, this);
    m_exec->setUseShell(true);

    m_exec->setWorkingDirectory(m_workspace);

    connect(m_exec, &KDevelop::CommandExecutor::completed, this, &ColconBuildJob::procFinished);
    connect(m_exec, &KDevelop::CommandExecutor::failed, this, &ColconBuildJob::procError);

    connect(m_exec, &KDevelop::CommandExecutor::receivedStandardError, model, &KDevelop::OutputModel::appendLines);
    connect(m_exec, &KDevelop::CommandExecutor::receivedStandardOutput, model, &KDevelop::OutputModel::appendLines);

    model->appendLine(QStringLiteral("%1> %2").arg(m_workspace, cmd));
    m_exec->start();
}

bool ColconBuildJob::doKill()
{
    m_killed = true;
    m_exec->kill();
    return true;
}

void ColconBuildJob::procError(QProcess::ProcessError error)
{
    if(!m_killed)
    {
        if(error == QProcess::FailedToStart)
        {
            setError(FailedToStart);
            setErrorText(i18n("Failed to start command."));
        }
        else if(error == QProcess::Crashed)
        {
            setError(Crashed);
            setErrorText(i18n("Command crashed."));
        }
        else
        {
            setError(UnknownExecError);
            setErrorText(i18n("Unknown error executing command."));
        }
    }

    emitResult();
}

void ColconBuildJob::procFinished(int exitcode)
{
    auto model = qobject_cast<KDevelop::OutputModel*>(OutputJob::model());

    if(exitcode != 0)
    {
        setError(FailedShownError);
        model->appendLine(i18n("*** Failed ***"));
    }
    else
        model->appendLine(i18n("*** Finished ***"));

    emitResult();
}
