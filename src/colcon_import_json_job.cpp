
#include "colcon_import_json_job.h"

#include "colcon_project_data.h"
#include <debug.h>

#include <language/duchain/duchain.h>
#include <language/duchain/duchainlock.h>
#include <interfaces/iproject.h>
#include <interfaces/icore.h>
#include <interfaces/iruntime.h>
#include <interfaces/iruntimecontroller.h>
#include <util/path.h>

#include <KShell>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QRegularExpression>

#include <wordexp.h>
#include <unistd.h>
#include <getopt.h>

using namespace KDevelop;

namespace {

enum class CmdParseState
{
    Default,
    Include,
    Ignore,
};

ColconFilesCompilationData importCommands(const Path& commandsFile)
{
    // NOTE: to get compile_commands.json, you need -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    QFile f(commandsFile.toLocalFile());
    bool r = f.open(QFile::ReadOnly|QFile::Text);
    if(!r) {
        qCWarning(COLCON) << "Couldn't open commands file" << commandsFile;
        return {};
    }

    qCDebug(COLCON) << "Found commands file" << commandsFile;

    ColconFilesCompilationData data;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(f.readAll(), &error);
    if (error.error) {
        qCWarning(COLCON) << "Failed to parse JSON in commands file:" << error.errorString() << commandsFile;
        data.isValid = false;
        return data;
    } else if (!document.isArray()) {
        qCWarning(COLCON) << "JSON document in commands file is not an array: " << commandsFile;
        data.isValid = false;
        return data;
    }

    const QString KEY_COMMAND = QStringLiteral("command");
    const QString KEY_DIRECTORY = QStringLiteral("directory");
    const QString KEY_FILE = QStringLiteral("file");
    auto rt = ICore::self()->runtimeController()->currentRuntime();
    const auto values = document.array();
    for (const QJsonValue& value : values) {
        if (!value.isObject()) {
            qCWarning(COLCON) << "JSON command file entry is not an object:" << value;
            continue;
        }
        const QJsonObject entry = value.toObject();
        if (!entry.contains(KEY_FILE) || !entry.contains(KEY_COMMAND) || !entry.contains(KEY_DIRECTORY)) {
            qCWarning(COLCON) << "JSON command file entry does not contain required keys:" << entry;
            continue;
        }

        QByteArray cmd = entry[KEY_COMMAND].toString().toUtf8();
        wordexp_t expanded{};
        if(wordexp(cmd.data(), &expanded, WRDE_NOCMD) != 0)
        {
            qCWarning(COLCON) << "wordexp error on string" << cmd;
            continue;
        }

        ColconFile ret;

        KDevelop::Path buildPath{entry[KEY_DIRECTORY].toString()};

        auto addInclude = [&](const QString& pathStr){
            if(pathStr.startsWith('/'))
                ret.includes << KDevelop::Path{pathStr};
            else
                ret.includes << KDevelop::Path{buildPath, pathStr};
        };

        CmdParseState state = CmdParseState::Default;

        qCDebug(COLCON) << "Parsing:" << cmd;
        for(std::size_t i = 1; i < expanded.we_wordc; ++i)
        {
            QString word = QString::fromUtf8(expanded.we_wordv[i]);

            switch(state)
            {
                case CmdParseState::Default:
                {
                    if(word.startsWith("-D"))
                    {
                        int idx = word.indexOf("=");
                        if(idx >= 0)
                            ret.defines[word.mid(2, idx-2)] = word.mid(idx+1);
                        else
                            ret.defines[word.mid(2)] = "";
                    }
                    else if(word.startsWith("-U"))
                    {
                        ret.defines.remove(word);
                    }
                    else if(word.startsWith("-I"))
                        addInclude(word.mid(2));
                    else if(word == "-isystem")
                        state = CmdParseState::Include;
                    else if(word.startsWith("-o"))
                    {
                        if(word == "-o")
                            state = CmdParseState::Ignore;
                    }
                    else if(word == "-c")
                    {
                    }
                    else if(word.startsWith('-'))
                    {
                        if(!ret.compileFlags.isEmpty())
                            ret.compileFlags += " ";

                        ret.compileFlags += word;
                    }

                    break;
                }
                case CmdParseState::Include:
                {
                    addInclude(word);
                    state = CmdParseState::Default;
                    break;
                }
                case CmdParseState::Ignore:
                {
                    state = CmdParseState::Default;
                    break;
                }
            }
        }

        wordfree(&expanded);

        const Path path(rt->pathInHost(Path(entry[KEY_FILE].toString())));
        qCDebug(COLCON) << "entering..." << path << entry[KEY_FILE];
        qCDebug(COLCON) << "compile flags:" << ret.compileFlags;
        qCDebug(COLCON) << "includes:" << ret.includes;
        qCDebug(COLCON) << "defines:" << ret.defines;

        if(ret.compileFlags.contains(".cpp.o"))
            std::abort();

        QHashIterator<QString, QString> it(ret.defines);
        while(it.hasNext())
        {
            it.next();
            if(it.key().contains(".cpp.o") || it.value().contains(".cpp.o"))
                std::abort();
        }

        for(auto& v : ret.includes)
        {
            if(v.toLocalFile().contains(".cpp.o"))
                std::abort();
        }

        if(entry[KEY_FILE].toString().contains("load/transit.cpp"))
            std::abort();

        data.files[path] = ret;
    }

//     std::abort();

    data.isValid = true;
    data.rebuildFileForFolderMapping();
    return data;
}

ImportData import(const Path& commandsFile)
{
    return ImportData {
        importCommands(commandsFile),
    };
}

}

ColconImportJsonJob::ColconImportJsonJob(IProject* project, QObject* parent)
    : KJob(parent)
    , m_project(project)
    , m_data({})
{
    connect(&m_futureWatcher, &QFutureWatcher<ImportData>::finished, this, &ColconImportJsonJob::importCompileCommandsJsonFinished);
}

ColconImportJsonJob::~ColconImportJsonJob()
{}

void ColconImportJsonJob::start()
{
    KDevelop::Path commandsFile(project()->path(), "../build/compile_commands.json");
    if (!QFileInfo::exists(commandsFile.toLocalFile()))
    {
        qCWarning(COLCON) << "Could not import Colcon project" << project()->path() << "('compile_commands.json' missing)";
        emitResult();
        return;
    }

    auto future = QtConcurrent::run(import, commandsFile);
    m_futureWatcher.setFuture(future);
}

void ColconImportJsonJob::importCompileCommandsJsonFinished()
{
    Q_ASSERT(m_project->thread() == QThread::currentThread());
    Q_ASSERT(m_futureWatcher.isFinished());

    auto future = m_futureWatcher.future();
    auto data = future.result();
    if (!data.compilationData.isValid)
    {
        qCWarning(COLCON) << "Could not import Colcon project ('compile_commands.json' invalid)";
        emitResult();
        return;
    }

    m_data = {data.compilationData};
    qCDebug(COLCON) << "Done importing, found" << data.compilationData.files.count() << "entries for" << project()->path();

    emitResult();
}

IProject* ColconImportJsonJob::project() const
{
    return m_project;
}

ColconProjectData ColconImportJsonJob::projectData() const
{
    Q_ASSERT(!m_futureWatcher.isRunning());
    return m_data;
}

#include "moc_colcon_import_json_job.cpp"
