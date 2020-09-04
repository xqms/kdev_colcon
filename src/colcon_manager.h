// KDevelop project manager for colcon projects
// Author: Max Schwarz <max.schwarz@ais.uni-bonn.de>

#ifndef KDEV_COLCON_H
#define KDEV_COLCON_H

#include <project/abstractfilemanagerplugin.h>
#include <project/interfaces/iprojectfilemanager.h>
#include <project/interfaces/ibuildsystemmanager.h>

#include <project/projectmodel.h>

class ColconManager
  : public KDevelop::AbstractFileManagerPlugin
  , public virtual KDevelop::IProjectFileManager
  , public virtual KDevelop::IBuildSystemManager
{
Q_OBJECT

Q_INTERFACES(KDevelop::IProjectFileManager)
Q_INTERFACES(KDevelop::IBuildSystemManager)

public:
    ColconManager(QObject* parent = nullptr, const QVariantList& args = QVariantList());
    ~ColconManager();

    KJob* createImportJob(KDevelop::ProjectFolderItem * item) override;
    KDevelop::ProjectFolderItem* import(KDevelop::IProject * project) override;
};

#endif
