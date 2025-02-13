#include "apk/titleitemsmodel.h"
#include <QFile>
#include <QDirIterator>
#include <QTextStream>
#include <QDebug>

TitleItemsModel::TitleItemsModel(const Project *apk, QObject *parent) : QAbstractTableModel(parent)
{
    // Parse application label attribute (android:label):

    QString labelAttribute = apk->getManifest()->scopes.first()->label().getValue();
    if (!labelAttribute.startsWith("@string/")) {
        return;
    }
    QString labelKey = labelAttribute.mid(QString("@string/").length());

    // Parse resource directories:

    QDirIterator resourceDirectories(apk->getContentsPath() + "/res/", QDir::Dirs | QDir::NoDotAndDotDot);
    while (resourceDirectories.hasNext()) {

        const QString resourceDirectory = QFileInfo(resourceDirectories.next()).fileName();
        const QString resourceType = resourceDirectory.split('-').first();

        if (resourceType == "values") {

            // Parse resource files:

            QDirIterator resourceFiles(apk->getContentsPath() + "/res/" + resourceDirectory, QDir::Files);
            while (resourceFiles.hasNext()) {
                const QString resourceFile = QFileInfo(resourceFiles.next()).filePath();
                add(resourceFile, labelKey);
            }
        }
    }
}

TitleItemsModel::~TitleItemsModel()
{
    qDeleteAll(nodes);
}

void TitleItemsModel::add(const QString &filepath, const QString &key)
{
    QFile xml(filepath);
    if (xml.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&xml);
        stream.setCodec("UTF-8");
        QDomDocument xmlDocument;
        xmlDocument.setContent(stream.readAll());

        // Iterate through child elements:

        QDomNodeList xmlNodes = xmlDocument.firstChildElement("resources").childNodes();
        for (int i = 0; i < xmlNodes.count(); ++i) {
            QDomElement xmlNode = xmlNodes.at(i).toElement();
            if (Q_LIKELY(!xmlNode.isNull())) {

                // Find application label nodes:

                if (xmlNode.nodeName() == "string" && xmlNode.attribute("name") == key) {
                    beginInsertRows(QModelIndex(), rowCount(), rowCount());
                        nodes.append(new TitleNode(new XmlNode(xmlNode, true), new ResourceFile(filepath)));
                    endInsertRows();
                }
            } else {
                qWarning() << "CRITICAL: Element \"resources\" contains non-element child nodes";
            }
        }
    }
}

bool TitleItemsModel::save() const
{
    for (const TitleNode *title : nodes) {
        title->save();
    }
    return true;
}

bool TitleItemsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.isValid() && role == Qt::EditRole) {
        const int row = index.row();
        TitleNode *title = nodes.at(row);
        if (title->node->getValue() != value) {
            title->node->setValue(value.toString());
            emit dataChanged(index, index);
            return true;
        }
    }
    return false;
}

QVariant TitleItemsModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        TitleNode *title = nodes.at(index.row());
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            switch (index.column()) {
                case Value:              return title->node->getValue();
                case ResourceLanguage:   return title->file->getLanguageName();
                case ResourceQualifiers: return title->file->getReadableQualifiers();
                case ResourcePath:       return title->file->getFilePath();
            }
        } else if (role == Qt::DecorationRole) {
            switch (index.column()) {
                case ResourceLanguage: return title->file->getLanguageIcon();
            }
        }
    }
    return QVariant();
}

QVariant TitleItemsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
            case Value:              return tr("Application Title");
            case ResourceLanguage:   return tr("Language");
            //: This string refers to the Android qualifiers (https://developer.android.com/guide/topics/resources/providing-resources).
            case ResourceQualifiers: return tr("Qualifiers");
            case ResourcePath:       return tr("Path");
        }
    }
    return QVariant();
}

QModelIndex TitleItemsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid())) {
        qWarning() << "CRITICAL: Unwanted parent passed to titles model";
        return QModelIndex();
    }
    return createIndex(row, column);
}

int TitleItemsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return nodes.count();
}

int TitleItemsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return ColumnCount;
}

Qt::ItemFlags TitleItemsModel::flags(const QModelIndex &index) const
{
    if (index.column() == 0) {
        return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
    } else {
        return QAbstractItemModel::flags(index);
    }
}
