// Import compile_commands.json
// Adapted from KDevelop's CMakeImportJsonJob (GPL)
//  by Max Schwarz <max.schwarz@ais.uni-bonn.de>

#ifndef COLCON_IMPORT_JSON_JOB_H
#define COLCON_IMPORT_JSON_JOB_H

#include "colcon_project_data.h"
#include <util/path.h>

#include <KJob>

#include <QFutureWatcher>

class ColconFolderItem;

struct ImportData {
    ColconFilesCompilationData compilationData;
};

namespace KDevelop
{
class IProject;
}

class ColconImportJsonJob : public KJob
{
Q_OBJECT

public:
    enum Error {
        FileMissingError = UserDefinedError, ///< JSON file was not found
        ReadError ///< Failed to read the JSON file
    };

    ColconImportJsonJob(KDevelop::IProject* project, QObject* parent);
    ~ColconImportJsonJob() override;

    void start() override;

    KDevelop::IProject* project() const;

    ColconProjectData projectData() const;

private Q_SLOTS:
    void importCompileCommandsJsonFinished();

private:
    KDevelop::IProject* m_project;
    QFutureWatcher<ImportData> m_futureWatcher;

    ColconProjectData m_data;
};

#endif // COLCON_IMPORT_JSON_JOB_H
