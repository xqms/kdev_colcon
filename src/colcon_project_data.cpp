
#include "colcon_project_data.h"

#include <KDirWatch>

bool operator==(const ColconFile& a, const ColconFile& b)
{
    return
        a.compileFlags == b.compileFlags
        && a.defines == b.defines
        && a.frameworkDirectories == b.frameworkDirectories
        && a.includes == b.includes
        && a.language == b.language;
}

void ColconFilesCompilationData::rebuildFileForFolderMapping()
{
    fileForFolder.clear();
    // iterate over files and add all direct folders
    for (auto it = files.constBegin(), end = files.constEnd(); it != end; ++it) {
        const auto file = it.key();
        const auto folder = file.parent();
        if (fileForFolder.contains(folder))
            continue;
        fileForFolder.insert(folder, it.key());
    }
    // now also add the parents of these folders
    const auto copy = fileForFolder;
    for (auto it = copy.begin(), end = copy.end(); it != end; ++it) {
        auto folder = it.key();
        while (folder.hasParent()) {
            folder = folder.parent();
            if (fileForFolder.contains(folder)) {
                break;
            }
            fileForFolder.insert(folder, it.key());
        }
    }
}

ColconProjectData::~ColconProjectData() = default;
