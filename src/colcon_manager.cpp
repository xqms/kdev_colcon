// KDevelop project manager for colcon projects
// Author: Max Schwarz <max.schwarz@ais.uni-bonn.de>

#include "colcon_manager.h"

#include <interfaces/iproject.h>

#include <debug.h>

#include <QMessageBox>
#include <QFileInfo>

#include <KPluginFactory>

K_PLUGIN_FACTORY_WITH_JSON(kdev_colconFactory, "kdev_colcon.json", registerPlugin<kdev_colcon>(); )

ColconManager::ColconManager(QObject* parent, const QVariantList&)
 : KDevelop::AbstractFileManagerPlugin(QStringLiteral("kdev_colcon"), parent)
{
}

ColconManager::~ColconManager()
{
}

KDevelop::ProjectFolderItem* ColconManager::import(KDevelop::IProject * project)
{
    const Path dir = project->path();

    if(!dir.isLocalFile())
    {
        QMessageBox::critical(nullptr, "Colcon Manager Error", "Colcon manager only supports local files.");
        return nullptr;
    }

    const Path jsonPath = dir / ".." / "build" / "compile_commands.json";

    if(!QFileInfo(jsonPath.toLocalFile()).exists())
    {
        QMessageBox::critical(nullptr, "Colcon Manager Error", "Could not find compile_commands.json. Did you compile with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON?");
        return nullptr;
    }

    return AbstractFileManagerPlugin::import(project);
}

KJob* ColconManager::createImportJob(KDevelop::ProjectFolderItem* item)
{
	auto project = item->project();

    connect(job, &CMakeImportJsonJob::result, this, [this, job]() {
            if (job->error() == 0) {
                manager->integrateData(job->projectData(), job->project());
            }
        });

	const QList<KJob*> jobs = {
		job,
		KDevelop::AbstractFileManagerPlugin::createImportJob(item) // generate the file system listing
	};

	Q_ASSERT(!jobs.contains(nullptr));
	ExecuteCompositeJob* composite = new ExecuteCompositeJob(this, jobs);
	//     even if the cmake call failed, we want to load the project so that the project can be worked on
	composite->setAbortOnError(false);
	return composite;
}

// needed for QObject class created from K_PLUGIN_FACTORY_WITH_JSON
#include "colcon_manager.moc"
