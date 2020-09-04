// KDevelop project manager for colcon projects
// Author: Max Schwarz <max.schwarz@ais.uni-bonn.de>

#ifndef KDEV_COLCON_H
#define KDEV_COLCON_H

#include <project/abstractfilemanagerplugin.h>
#include <project/interfaces/iprojectfilemanager.h>
#include <project/interfaces/ibuildsystemmanager.h>

#include <project/projectmodel.h>

class ColconProjectData;
class ColconFile;

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

    KDevelop::Path::List includeDirectories(KDevelop::ProjectBaseItem *item) const override;
    KDevelop::Path::List frameworkDirectories(KDevelop::ProjectBaseItem *item) const override;
    QHash<QString, QString> defines(KDevelop::ProjectBaseItem *item) const override;
    QString extraArguments(KDevelop::ProjectBaseItem* item) const override;
    KDevelop::Path compiler(KDevelop::ProjectTargetItem* p) const override;

    KDevelop::ProjectTargetItem* createTarget(const QString& target, KDevelop::ProjectFolderItem *parent) override;

    bool removeTarget(KDevelop::ProjectTargetItem* target) override;

    QList<KDevelop::ProjectTargetItem*> targets(KDevelop::ProjectFolderItem*) const override;

    bool addFilesToTarget(
        const QList<KDevelop::ProjectFileItem*>& files,
        KDevelop::ProjectTargetItem* target
    ) override;

    bool removeFilesFromTargets(
        const QList<KDevelop::ProjectFileItem*> &files
    ) override;

    bool hasBuildInfo(KDevelop::ProjectBaseItem* item) const override;

    KDevelop::Path buildDirectory(KDevelop::ProjectBaseItem*) const override;

    KDevelop::IProjectBuilder* builder() const override;

private:
    void integrateData(const ColconProjectData& data, KDevelop::IProject* project);
    ColconFile fileInformation(KDevelop::ProjectBaseItem* item) const;

    QHash<KDevelop::IProject*, ColconProjectData> m_projectData;
};

#endif
