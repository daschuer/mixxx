#include "preferences/effectchainpresetlistmodel.h"

#include <QMimeData>

namespace {
const QString kMimeTextDelimiter = QStringLiteral("\n");
} // anonymous namespace

QMimeData* EffectChainPresetListModel::mimeData(const QModelIndexList& indexes) const {
    QMimeData* mimeData = new QMimeData();
    QStringList chainNameList;
    for (const auto& index : indexes) {
        if (index.isValid()) {
            chainNameList << data(index).toString();
        }
    }
    mimeData->setText(chainNameList.join(kMimeTextDelimiter));
    return mimeData;
}

bool EffectChainPresetListModel::dropMimeData(
        const QMimeData* data,
        Qt::DropAction action,
        int row,
        int column,
        const QModelIndex& parent) {
    Q_UNUSED(column);
    Q_UNUSED(action);
    if (!data->hasText()) {
        return false;
    }
    if (row == -1) {
        row = parent.row();
        // Dropping onto an empty model or dropping past the end of a model
        if (parent.row() == -1) {
            row = stringList().size();
        }
    }
    const QStringList mimeTextLines = data->text().split(kMimeTextDelimiter);
    QStringList chainList = stringList();
    for (const auto& line : mimeTextLines) {
        int oldIndex = chainList.indexOf(line);
        chainList.insert(row, line);
        // Remove duplicate
        if (oldIndex != -1) {
            if (row < oldIndex) {
                oldIndex++;
            }
            chainList.removeAt(oldIndex);
        }
    }
    setStringList(chainList);
    return true;
}

QStringList EffectChainPresetListModel::mimeTypes() const {
    return {QLatin1String("text/plain")};
}

Qt::DropActions EffectChainPresetListModel::supportedDropActions() const {
    // Qt::MoveAction would remove the chain from its original list when
    // dropping it on the other list, but we want to copy it. dropMimeData
    // is responsible for ensuring that duplicates are not created when dragging
    // and dropping within the same list.
    return Qt::CopyAction;
}

Qt::ItemFlags EffectChainPresetListModel::flags(const QModelIndex& index) const {
    Q_UNUSED(index);
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}
