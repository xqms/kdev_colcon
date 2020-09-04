

#ifndef COLCON_PROJECT_DATA_H
#define COLCON_PROJECT_DATA_H

#include <QSharedPointer>
#include <QStringList>
#include <QHash>
#include <util/path.h>
#include <QDebug>

/**
 * Contains the required information to compile it properly
 */
struct ColconFile
{
    KDevelop::Path::List includes;
    KDevelop::Path::List frameworkDirectories;
    QString compileFlags;
    QString language;
    QHash<QString, QString> defines;

    void addDefine(const QString& define);

    bool isEmpty() const
    {
        return includes.isEmpty() && frameworkDirectories.isEmpty()
            && compileFlags.isEmpty() && defines.isEmpty();
    }
};
Q_DECLARE_TYPEINFO(ColconFile, Q_MOVABLE_TYPE);

inline QDebug &operator<<(QDebug debug, const ColconFile& file)
{
    debug << "ColconFile(-I" << file.includes << ", -F" << file.frameworkDirectories << ", -D" << file.defines << ", " << file.language << ")";
    return debug.maybeSpace();
}

struct ColconFilesCompilationData
{
    QHash<KDevelop::Path, ColconFile> files;
    bool isValid = false;
    /// lookup table to quickly find a file path for a given folder path
    /// this greatly speeds up fallback searching for information on untracked files
    /// based on their folder path
    QHash<KDevelop::Path, KDevelop::Path> fileForFolder;
    void rebuildFileForFolderMapping();
};

struct ColconProjectData
{
    ColconFilesCompilationData compilationData;
};

#endif
