

#ifndef COLCON_PROJECT_DATA_H
#define COLCON_PROJECT_DATA_H

#include <QSharedPointer>
#include <QStringList>
#include <QHash>
#include <util/path.h>
#include <QDebug>
#include <QPointer>

class KDirWatch;

/**
 * Contains the required information to compile it properly
 */
class ColconFile
{
public:
    KDevelop::Path::List includes;
    KDevelop::Path::List frameworkDirectories;
    QString compileFlags;
    QString language;
    QHash<QString, QString> defines;

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

bool operator==(const ColconFile& a, const ColconFile& b);

inline bool operator!=(const ColconFile& a, const ColconFile& b)
{ return !(a == b); }

class ColconFilesCompilationData
{
public:
    QHash<KDevelop::Path, ColconFile> files;
    bool isValid = false;
    /// lookup table to quickly find a file path for a given folder path
    /// this greatly speeds up fallback searching for information on untracked files
    /// based on their folder path
    QHash<KDevelop::Path, KDevelop::Path> fileForFolder;
    void rebuildFileForFolderMapping();
};

class ColconProjectData
{
public:
    ColconProjectData() = default;
    ColconProjectData(const ColconProjectData&) = delete;
    ColconProjectData(ColconProjectData&&) = default;
    ~ColconProjectData();

    ColconProjectData& operator=(const ColconProjectData&) = delete;
    ColconProjectData& operator=(ColconProjectData&&) = default;

    ColconFilesCompilationData compilationData;

    QPointer<KDirWatch> jsonWatcher;
};

#endif
