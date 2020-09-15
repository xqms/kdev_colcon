// KDevelop project manager for colcon projects
// Author: Max Schwarz <max.schwarz@ais.uni-bonn.de>

#include "colcon_manager.h"

#include "colcon_import_json_job.h"
#include "colcon_build_job.h"

#include <interfaces/icore.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iruncontroller.h>
#include <util/executecompositejob.h>

#include <debug.h>

#include <QMessageBox>
#include <QFileInfo>

#include <KDirWatch>
#include <KPluginFactory>

#include <memory>

K_PLUGIN_FACTORY_WITH_JSON(kdev_colconFactory, "kdev_colcon.json", registerPlugin<ColconManager>(); )

ColconManager::ColconManager(QObject* parent, const QVariantList&)
 : KDevelop::AbstractFileManagerPlugin(QStringLiteral("kdev_colcon"), parent)
{
    connect(
        KDevelop::ICore::self()->projectController(),
            &KDevelop::IProjectController::projectClosing,
        this,
            &ColconManager::projectClosing
    );
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

    const KDevelop::Path jsonPath(project->path(),  "../build/compile_commands.json");

    auto job = new ColconImportJsonJob(jsonPath.toLocalFile(), this);
    connect(job, &ColconImportJsonJob::result, this, [this, job, project]() {
        if (job->error() == 0)
        {
            integrateData(job->data(), project);
        }
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

bool ColconManager::integrateData(const ColconFilesCompilationData& data, KDevelop::IProject* project)
{
    auto it = m_projectData.find(project);

    if(it == m_projectData.end())
    {
        auto projectData = std::make_unique<ColconProjectData>();

        projectData->compilationData = std::move(data);

        const KDevelop::Path jsonPath(project->path(),  "../build/compile_commands.json");

        projectData->jsonWatcher = new KDirWatch();
        projectData->jsonWatcher->addFile(jsonPath.toLocalFile());

        connect(projectData->jsonWatcher, &KDirWatch::dirty, [this, jsonPath, project](){
            qCWarning(COLCON) << "DirWatch says JSON has changed!";
            auto job = new ColconImportJsonJob(jsonPath.toLocalFile(), this);
            connect(job, &ColconImportJsonJob::result, this, [this, job, project]() {
                if(job->error() == 0)
                {
                    if(integrateData(job->data(), project))
                    {
                        qCDebug(COLCON) << "Triggering reparse...";
                        emit KDevelop::ICore::self()->projectController()->projectConfigurationChanged(project);
                        KDevelop::ICore::self()->projectController()->reparseProject(project);
                    }
                }
            });

            project->setReloadJob(job);
            KDevelop::ICore::self()->runController()->registerJob(job);
        });

        m_projectData[project] = std::move(projectData);

        return true;
    }
    else
    {
        auto& projectData = it->second;
        auto& currentFiles = projectData->compilationData.files;

        bool changed = false;

        QHashIterator<KDevelop::Path, ColconFile> fileIt(data.files);
        while(fileIt.hasNext())
        {
            fileIt.next();

            auto cFileIt = currentFiles.find(fileIt.key());
            if(cFileIt == currentFiles.end())
            {
                currentFiles[fileIt.key()] = fileIt.value();
                changed = true;
            }
            else
            {
                auto& currentValue = cFileIt.value();
                if(currentValue != fileIt.value())
                {
                    currentValue = fileIt.value();
                    changed = true;
                }
            }
        }

        if(changed)
        {
            qCWarning(COLCON) << "JSON changed";
            projectData->compilationData.rebuildFileForFolderMapping();
        }

        return changed;
    }
}

ColconFile ColconManager::fileInformation(KDevelop::ProjectBaseItem* item) const
{
    auto it = m_projectData.find(item->project());
    if(it == m_projectData.end())
        return {};

    const auto& data = it->second->compilationData;

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
    auto it = m_projectData.find(item->project());
    if(it == m_projectData.end())
        return false;

    return it->second->compilationData.files.contains(item->path());
}

KDevelop::IProjectBuilder* ColconManager::builder() const
{
    return const_cast<IProjectBuilder*>(static_cast<const IProjectBuilder*>(this));
}

KDevelop::Path ColconManager::buildDirectory(KDevelop::ProjectBaseItem*) const
{
    return {};
}

bool ColconManager::reload(KDevelop::ProjectFolderItem* folder)
{
    qCDebug(COLCON) << "reloading" << folder->path();

    KDevelop::IProject* project = folder->project();
    if(!project->isReady())
        return false;

    KJob *job = createImportJob(folder);
    project->setReloadJob(job);
    KDevelop::ICore::self()->runController()->registerJob( job );
    if(folder == project->projectItem())
    {
        connect(job, &KJob::finished, this, [project](KJob* job) {
            if (job->error())
                return;

            emit KDevelop::ICore::self()->projectController()->projectConfigurationChanged(project);
            KDevelop::ICore::self()->projectController()->reparseProject(project);
        });
    }

    return true;
}

void ColconManager::projectClosing(KDevelop::IProject* project)
{
    m_projectData.erase(project);
}

KJob* ColconManager::build(KDevelop::ProjectBaseItem* item)
{
    return new ColconBuildJob(item->project(), this);
}

KJob* ColconManager::install(KDevelop::ProjectBaseItem* item, const QUrl& specificPrefix)
{
    Q_UNUSED(item);
    Q_UNUSED(specificPrefix);
    return nullptr;
}

KJob* ColconManager::clean(KDevelop::ProjectBaseItem* item)
{
    Q_UNUSED(item);
    return nullptr;
}

#include "colcon_manager.moc"
