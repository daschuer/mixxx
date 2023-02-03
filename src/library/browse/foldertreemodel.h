#pragma once

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QVariant>
#include <QList>
#include <QHash>

#include "library/treeitemmodel.h"
#include "library/treeitem.h"

class TreeItem;
// This class represents a folder item within the Browse Feature
// The class is derived from TreeItemModel to support lazy model
// initialization.

class FolderTreeModel : public TreeItemModel {
    Q_OBJECT
  public:
    FolderTreeModel(QObject *parent = 0);
    ~FolderTreeModel() override = default;
    bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
    bool directoryHasChildren(const QString& path) const;

  private:
    // Used for memorizing the results of directoryHasChildren
    mutable QHash<QString, bool> m_directoryCache;
};
