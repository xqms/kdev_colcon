// KDevelop project manager for colcon projects
// Author: Max Schwarz <max.schwarz@ais.uni-bonn.de>

#include "colcon_manager.h"

#include "colcon_import_json_job.h"

#include <interfaces/iproject.h>
#include <util/executecompositejob.h>

#include <debug.h>

#include <QMessageBox>
#include <QFileInfo>

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(kdev_colconFactory, "kdev_colcon.json", registerPlugin<ColconManager>(); )

ColconManager::ColconManager(QObject* parent, const QVariantList&)
 : KDevelop::AbstractFileManagerPlugin(QStringLiteral("kdev_colcon"), parent)
{
}

ColconManager::~ColconManager()
{
}

KDevelop::ProjectFolderItem* ColconManager::import(KDevelop::IProject * project)
{
    const KDevelop::Path dir = project->path();

    if(!dir.isLocalFile())
    {
        QMessageBox::critical(nullptr, "Colcon Manager Error", "Colcon manager only supports local files.");
        return nullptr;
    }

    const KDevelop::Path jsonPath(dir,  "../build/compile_commands.json");

    if(!QFileInfo(jsonPath.toLocalFile()).exists())
    {
        qCWarning(COLCON) << "Could not open file " << jsonPath;
        QMessageBox::critical(nullptr, "Colcon Manager Error", "Could not find compile_commands.json. Did you compile with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON?");
        return nullptr;
    }

    return AbstractFileManagerPlugin::import(project);
}

KJob* ColconManager::createImportJob(KDevelop::ProjectFolderItem* item)
{
    auto project = item->project();

    auto job = new ColconImportJsonJob(project, this);
    connect(job, &ColconImportJsonJob::result, this, [this, job]() {
        if (job->error() == 0)
            integrateData(job->projectData(), job->project());
    });

    const QList<KJob*> jobs = {
        job,
        KDevelop::AbstractFileManagerPlugin::createImportJob(item) // generate the file system listing
    };

    Q_ASSERT(!jobs.contains(nullptr));
    KDevelop::ExecuteCompositeJob* composite = new KDevelop::ExecuteCompositeJob(this, jobs);
    composite->setAbortOnError(false);
    return composite;
}

void ColconManager::integrateData(const ColconProjectData& data, KDevelop::IProject* project)
{
    m_projectData[project] = data;
}

ColconFile ColconManager::fileInformation(KDevelop::ProjectBaseItem* item) const
{
    const auto& data = m_projectData[item->project()].compilationData;

    auto toCanonicalPath = [](const KDevelop::Path &path) -> KDevelop::Path {
        // if the path contains a symlink, then we will not find it in the lookup table
        // as that only only stores canonicalized paths. Thus, we fallback to
        // to the canonicalized path and see if that brings up any matches
        const auto localPath = path.toLocalFile();
        const auto canonicalPath = QFileInfo(localPath).canonicalFilePath();
        return (localPath == canonicalPath) ? path : KDevelop::Path(canonicalPath);
    };

    auto path = item->path();

    if (!item->folder()) {
        // try to look for file meta data directly
        auto it = data.files.find(path);
        if (it == data.files.end()) {
            // fallback to canonical path lookup
            auto canonical = toCanonicalPath(path);
            if (canonical != path) {
                it = data.files.find(canonical);
            }
        }
        if (it != data.files.end()) {
            return *it;
        }
        // else look for a file in the parent folder
        path = path.parent();
    }

    while (true) {
        // try to look for a file in the current folder path
        auto it = data.fileForFolder.find(path);
        if (it == data.fileForFolder.end()) {
            // fallback to canonical path lookup
            auto canonical = toCanonicalPath(path);
            if (canonical != path) {
                it = data.fileForFolder.find(canonical);
            }
        }
        if (it != data.fileForFolder.end()) {
            return data.files[it.value()];
        }
        if (!path.hasParent()) {
            break;
        }
        path = path.parent();
    }

    qCDebug(COLCON) << "no information found for" << item->path();
    return {};
}

KDevelop::Path::List ColconManager::includeDirectories(KDevelop::ProjectBaseItem *item) const
{
    return fileInformation(item).includes;
}

KDevelop::Path::List ColconManager::frameworkDirectories(KDevelop::ProjectBaseItem *item) const
{
    return fileInformation(item).frameworkDirectories;
}

QHash<QString, QString> ColconManager::defines(KDevelop::ProjectBaseItem *item ) const
{
    return fileInformation(item).defines;
}

QString ColconManager::extraArguments(KDevelop::ProjectBaseItem *item) const
{
    return fileInformation(item).compileFlags;
}

KDevelop::Path ColconManager::compiler(KDevelop::ProjectTargetItem*) const
{
    return {};
}


KDevelop::ProjectTargetItem * ColconManager::createTarget(const QString&, KDevelop::ProjectFolderItem*)
{
	return 0;
}

QList<KDevelop::ProjectTargetItem *> ColconManager::targets(KDevelop::ProjectFolderItem*) const
{
	return {};
}

bool ColconManager::addFilesToTarget(const QList<KDevelop::ProjectFileItem *>&, KDevelop::ProjectTargetItem*)
{
	return false;
}

bool ColconManager::removeFilesFromTargets(const QList<KDevelop::ProjectFileItem *>&)
{
	return false;
}

bool ColconManager::removeTarget(KDevelop::ProjectTargetItem*)
{
	return false;
}

bool ColconManager::hasBuildInfo(KDevelop::ProjectBaseItem* item) const
{
	return m_projectData[item->project()].compilationData.files.contains(item->path());
}

KDevelop::IProjectBuilder* ColconManager::builder() const
{
    return nullptr;
}

KDevelop::Path ColconManager::buildDirectory(KDevelop::ProjectBaseItem*) const
{
    return {};
}


#include "colcon_manager.moc"
