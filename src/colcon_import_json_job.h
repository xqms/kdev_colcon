// Import compile_commands.json
// Adapted from KDevelop's CMakeImportJsonJob (GPL)
//  by Max Schwarz <max.schwarz@ais.uni-bonn.de>

#ifndef COLCON_IMPORT_JSON_JOB_H
#define COLCON_IMPORT_JSON_JOB_H

#include "colcon_project_data.h"
#include <util/path.h>

#include <KJob>

#include <QFutureWatcher>

class ColconImportJsonJob : public KJob
{
Q_OBJECT

public:
    enum Error {
        FileMissingError = UserDefinedError, ///< JSON file was not found
        ReadError ///< Failed to read the JSON file
    };

    ColconImportJsonJob(const QString& filename, QObject* parent);
    ~ColconImportJsonJob() override;

    void start() override;

    const ColconFilesCompilationData& data() const;

private Q_SLOTS:
    void importCompileCommandsJsonFinished();

private:
    QString m_filename;
    QFutureWatcher<ColconFilesCompilationData> m_futureWatcher;

    ColconFilesCompilationData m_data;
};

#endif // COLCON_IMPORT_JSON_JOB_H
