// Build job
// Author: Max Schwarz <max.schwarz@online.de>

#include "colcon_build_job.h"

#include <KLocalizedString>

#include <interfaces/iproject.h>
#include <outputview/outputdelegate.h>
#include <outputview/outputmodel.h>
#include <outputview/outputfilteringstrategies.h>
#include <util/commandexecutor.h>
#include <util/environmentprofilelist.h>
#include <util/path.h>

#include <debug.h>

ColconBuildJob::ColconBuildJob(KDevelop::IProject* project, QObject* parent)
 : OutputExecuteJob{parent}
{
    setToolTitle(i18n("Colcon"));
    setCapabilities(Killable);
    setStandardToolView(KDevelop::IOutputView::BuildView);
    setBehaviours(KDevelop::IOutputView::AllowUserClose | KDevelop::IOutputView::AutoScroll);
    setFilteringStrategy(KDevelop::OutputModel::CompilerFilter);
    setProperties(NeedWorkingDirectory | PortableMessages | DisplayStderr | IsBuilderHint | PostProcessOutput);

    // We want to get feedback immediately, so switch off line buffering
    addEnvironmentOverride(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));

    // We need a post-processing step with tr, since colcon separates its status
    // line prints with \r, which is not recognized as a line delimiter by
    // KDevelop's line splitter...
    *this << "bash"
        << "-c"
        << ". /opt/ros/melodic/setup.sh && stdbuf -o0 colcon build"
        " --symlink-install"
        " --event-handlers status+ console_start_end-"
        " --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo"
        " | stdbuf -o0 tr '\\r' '\\n'";

    QString title = i18nc("Building: <project name>", "Building: %1", project->name());
    setJobName(title);

    setWorkingDirectory(project->path().parent().toUrl());
}

void ColconBuildJob::postProcessStderr(const QStringList& lines)
{
    appendLines(lines);
}

void ColconBuildJob::postProcessStdout(const QStringList& lines)
{
    appendLines(lines);
}

void ColconBuildJob::appendLines(const QStringList& lines)
{
    static const QRegularExpression re(QStringLiteral(
        R"EOS(^\[[^\]]+\] \[([0-9]+)\/([0-9]+) complete\](.*))EOS"));

    QStringList ret(lines);
    for(QStringList::iterator it = ret.begin(); it != ret.end(); )
    {
        if(it->trimmed().isEmpty())
            it = ret.erase(it);
        else if(it->startsWith(QLatin1Char('[')))
        {
            const QString& line = *it;
            QRegularExpressionMatch match = re.match(line.trimmed());
            if(match.hasMatch())
            {
                const int current = match.capturedRef(1).toInt();
                const int total = match.capturedRef(2).toInt();
                const QString action = match.captured(3);

                emitPercent(current, total);
                infoMessage(this, action);
            }
            else
                qCDebug(COLCON) << "Could not understand status line:" << line;

            it = ret.erase(it);
        }
        else
            it++;
    }

    model()->appendLines(ret);
}
